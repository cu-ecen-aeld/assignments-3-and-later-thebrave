
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/types.h>
#include <unistd.h>

#define SMALL_BUF_SIZE 256

bool should_die = false;

void print_ipv4_info(struct sockaddr *addr, const char *prefix) {
  // Cast generic sockaddr to IPv4-specific sockaddr_in
  struct sockaddr_in *ipv4 = (struct sockaddr_in *)addr;

  // Buffer for the IP string
  char ip_str[INET_ADDRSTRLEN];

  // Convert binary IP to string
  if (inet_ntop(AF_INET, &(ipv4->sin_addr), ip_str, INET_ADDRSTRLEN) != NULL) {
    // Convert port from network to host byte order
    uint16_t port = ntohs(ipv4->sin_port);
    printf("%s %s:%d\n", prefix, ip_str, port);
    syslog(LOG_INFO, "%s %s:%d", prefix, ip_str, port);
  } else {
    perror("inet_ntop");
  }
}

static void signal_handler(int signo) {
  if (signo == SIGINT || signo == SIGTERM) {
    syslog(LOG_INFO, "Caught signal, exiting");
    should_die = true;
  } else {
    syslog(LOG_ERR, "Caught unknown signal %d", signo);
  }
}

int main(int argc, char *argv[]) {
  struct sigaction act;
  memset(&act, 0, sizeof(act));
  act.sa_handler = signal_handler;

  sigaction(SIGINT, &act, NULL);
  sigaction(SIGTERM, &act, NULL);

  int sockfd = socket(PF_INET, SOCK_STREAM, 0);
  if (sockfd == -1) {
    perror("socket");
    return -1;
  }

  struct addrinfo hints;
  struct addrinfo *result;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;       /* Allow IPv4 or IPv6 */
  hints.ai_socktype = SOCK_STREAM; /* Datagram socket */
  int status = getaddrinfo("127.0.0.1", "9000", &hints, &result);

  if (status != 0) {
    perror("getaddrinfo");
    status = -1;
    goto close_socket;
  }
  status = bind(sockfd, result->ai_addr, result->ai_addrlen);
  if (status == -1) {
    perror("bind");
    freeaddrinfo(result);
    goto close_socket;
  }
  freeaddrinfo(result);

  status = listen(sockfd, 0);
  if (status == -1) {
    perror("listen");
    goto close_socket;
  }

  struct sockaddr cnx_addr;
  socklen_t cnx_addrlen = sizeof(struct sockaddr);
  int output_file = -1;

  while (!should_die) {
    int cnx_fd = accept(sockfd, &cnx_addr, &cnx_addrlen);
    if (cnx_fd == -1) {
      if (errno == EINTR) break; 
      perror("accept");
      status = -1;
      goto close_socket;
    }
    print_ipv4_info(&cnx_addr, "Accepted connection from");

    // Open the output file, creating this file if it doesn’t exist
    output_file = open("/var/tmp/aesdsocketdata", O_RDWR | O_CREAT | O_APPEND,
             S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (output_file == -1) {
      perror("open");
      close(cnx_fd);
      status = -1;
      goto close_socket;
    }

    // Socket is open, loop until stream ends
    char *buffer = NULL;
    off_t file_size = 0;
    char *tmp_buffer = malloc(SMALL_BUF_SIZE);
    memset(tmp_buffer, 0, SMALL_BUF_SIZE);

    while (!should_die) {
      memset(tmp_buffer, 0, SMALL_BUF_SIZE);
      printf("[d] Blocking read\n");

      // Blocking read from the socket
      ssize_t bytes_read = read(cnx_fd, tmp_buffer, SMALL_BUF_SIZE - 1);
      if (bytes_read < 1) {
        printf("[d] Connection closed\n");
        break;
      }

      printf("[d] Received %zd bytes: \"%s\"\n", bytes_read, tmp_buffer);

      // Do I need to initialize a buffer ?
      if (buffer == NULL) {
        buffer = malloc(strlen(tmp_buffer) + 1);
        strcpy(buffer, tmp_buffer);
        buffer[strlen(tmp_buffer)] = '\0';
      } else {
        // If not, then I'll have to append to that buffer
        buffer = realloc(buffer, strlen(buffer) + strlen(tmp_buffer) + 1);
        strcat(buffer, tmp_buffer);
        buffer[strlen(buffer)] = '\0';
      }

      printf("[d] Current buffer %zd bytes: \"%s\"\n", strlen(buffer), buffer);

      // If there is a \n in the string,
      char *pos = strchr(buffer, '\n');
      if (pos != NULL) {
        printf("[d] Found a newline\n");
        ssize_t len = pos - buffer + 1; // Include newline
        ssize_t bytes_written = write(output_file, buffer, len);
        if (bytes_written < 1) {
          perror("write");
          goto inner_cleanup;
        }
        file_size += len;

        assert(bytes_written == len);
        printf("[d] Wrote %zd bytes to file\n", bytes_written);
        break;
      }
    }
    
    if (buffer) {
        free(buffer);
        buffer = NULL;
    }

    printf("[d] Preparing to write to file\n");
    off_t res = lseek(output_file, 0, SEEK_SET);
    if (res == -1) {
      perror("lseek");
      goto inner_cleanup;
    }
    printf("[d] Position %zd bytes to file\n", res);

    // Copy Loop
    ssize_t bytes_read, bytes_written;

    printf("[d] Write loop\n");
    while (true) {
      memset(tmp_buffer, 0, SMALL_BUF_SIZE);

      bytes_read = read(output_file, tmp_buffer, SMALL_BUF_SIZE);
      printf("[d] Read %zd bytes\n", bytes_read);
      if (bytes_read == 0) {
        // EOF
        break;
      } else if (bytes_read < 0) {
        perror("read");
        goto inner_cleanup;
      }

      // Loop to handle partial writes
      ssize_t total_written = 0;
      while (total_written < bytes_read) {
        bytes_written = write(cnx_fd, tmp_buffer + total_written,
                              bytes_read - total_written);
        printf("[d] Wrote %zd/%zd bytes\n", bytes_written, total_written);

        if (bytes_written < 0) {
          // If interrupted by signal, retry; otherwise handle error
          if (errno == EINTR)
            continue;
          goto inner_cleanup;
        }
        total_written += bytes_written;
      }
    }

inner_cleanup:
    free(tmp_buffer);
    // Close connexion
    close(cnx_fd);
    close(output_file);
    printf("[d] Done writing\n");
    print_ipv4_info(&cnx_addr, "Closed connection from");
  }

close_socket:
  close(sockfd);
  return status;
}