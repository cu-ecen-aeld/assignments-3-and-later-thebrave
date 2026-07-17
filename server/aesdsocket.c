
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
#include <sys/stat.h>
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

  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) <
      0) {
    perror("setsockopt(SO_REUSEADDR) failed");
    return -1;
  }

  struct sockaddr_in server_addr;
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(9000);
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

  int status =
      bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));

  if (status == -1) {
    perror("bind");
    goto close_socket;
  }

  // Check if should daemonize
  if (argc > 1 && strcmp(argv[1], "-d") == 0) {
    pid_t pid = fork();
    if (pid < 0) {
      perror("fork");
      goto close_socket;
    } else if (pid > 0) {
      exit(EXIT_SUCCESS);
    }

    setsid();
    chdir("/");
    umask(0);
    // close(STDIN_FILENO);
    // close(STDOUT_FILENO);
    // close(STDERR_FILENO);
  }

  status = listen(sockfd, 10);
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
    char *tmp_buffer = malloc(SMALL_BUF_SIZE);
    memset(tmp_buffer, 0, SMALL_BUF_SIZE);

    while (!should_die) {
      memset(tmp_buffer, 0, SMALL_BUF_SIZE);
      syslog(LOG_INFO, "[d] Blocking read");

      // Blocking read from the socket
      ssize_t bytes_read = read(cnx_fd, tmp_buffer, SMALL_BUF_SIZE - 1);
      if (bytes_read < 1) {
        syslog(LOG_INFO, "[d] Connection closed");
        break;
      }

      syslog(LOG_INFO, "[d] Received %zd bytes: \"%s\"", bytes_read,
             tmp_buffer);

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

      syslog(LOG_INFO, "[d] Current buffer %zd bytes: \"%s\"", strlen(buffer),
             buffer);

      // If there is a \n in the string,
      char *pos = strchr(buffer, '\n');
      if (pos != NULL) {
        syslog(LOG_INFO, "[d] Found a newline");
        ssize_t len = pos - buffer + 1;  // Include newline
        ssize_t bytes_written = write(output_file, buffer, len);
        if (bytes_written < 1) {
          perror("write");
          goto inner_cleanup;
        }

        assert(bytes_written == len);
        syslog(LOG_INFO, "[d] Wrote %zd bytes to file", bytes_written);
        break;
      }
    }

    // Cleanup
    free(buffer);
    buffer = NULL;

    syslog(LOG_INFO, "[d] Preparing to write to file");
    off_t res = lseek(output_file, 0, SEEK_SET);
    if (res == -1) {
      perror("lseek");
      goto inner_cleanup;
    }
    syslog(LOG_INFO, "[d] Position %zd bytes to file", res);

    // Copy Loop
    ssize_t bytes_read, bytes_written;

    syslog(LOG_INFO, "[d] Write loop");
    while (true) {
      memset(tmp_buffer, 0, SMALL_BUF_SIZE);

      bytes_read = read(output_file, tmp_buffer, SMALL_BUF_SIZE);
      syslog(LOG_INFO, "[d] Read %zd bytes", bytes_read);
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
        syslog(LOG_INFO, "[d] Wrote %zd/%zd bytes", bytes_written,
               total_written);

        if (bytes_written < 0) {
          // If interrupted by signal, retry; otherwise handle error
          if (errno == EINTR) continue;
          goto inner_cleanup;
        }
        total_written += bytes_written;
      }
    }

  inner_cleanup:
    free(tmp_buffer);
    // Close connexion
    close(cnx_fd);

    syslog(LOG_INFO, "[d] Done writing");
    print_ipv4_info(&cnx_addr, "Closed connection from");
  }

  close(output_file);
  remove("/var/tmp/aesdsocketdata");

close_socket:
  close(sockfd);
  return status;
}