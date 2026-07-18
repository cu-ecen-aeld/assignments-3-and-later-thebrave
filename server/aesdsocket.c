
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
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syslog.h>
#include <sys/types.h>
#include <unistd.h>

#include "thr_cnx.h"
#include "thr_cst.h"
#include "thr_tim.h"
#include "util.h"

bool should_die = false;

struct entry {
  pthread_t thread_id;
  SLIST_ENTRY(entry) entries; /* Singly linked list */
};

SLIST_HEAD(slisthead, entry);

static void signal_handler(int signo) {
  if (signo == SIGINT || signo == SIGTERM) {
    syslog(LOG_INFO, "Caught signal, exiting");
    should_die = true;
  } else {
    syslog(LOG_ERR, "Caught unknown signal %d", signo);
  }
}

int main(int argc, char *argv[]) {
  void *output_args = NULL;
  struct slisthead head; /* Singly linked list
                          head */

  SLIST_INIT(&head); /* Initialize the queue */

  // Set up signal handling for SIGINT and SIGTERM
  struct sigaction act;
  memset(&act, 0, sizeof(act));
  act.sa_handler = signal_handler;

  sigaction(SIGINT, &act, NULL);
  sigaction(SIGTERM, &act, NULL);

  // Create a TCP socket
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

  // Bind the socket to the specified port and address
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
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
  }

  status = listen(sockfd, 10);
  if (status == -1) {
    perror("listen");
    goto close_socket;
  }

  // Open the output file, creating this file if it doesn’t exist
  int output_file = open("/var/tmp/aesdsocketdata", O_RDWR | O_CREAT | O_APPEND,
                         S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  if (output_file == -1) {
    perror("open");
    status = -1;
    goto close_socket;
  }

  pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;
  timer_t timerid = timer_init(&file_mutex, output_file, &output_args);
  if (timerid == NULL) {
    perror("timer_init");
    status = -1;
    goto close_socket;
  }

  while (!should_die) {
    struct thread_args *args = malloc(sizeof(struct thread_args));
    args->cnx_addrlen = sizeof(struct sockaddr);
    args->file_mutex = &file_mutex;
    args->output_file = output_file;

    args->cnx_fd = accept(sockfd, &args->cnx_addr, &args->cnx_addrlen);
    if (args->cnx_fd == -1) {
      free(args);
      if (errno == EINTR) break;
      perror("accept");
      status = -1;
      goto close_socket;
    }

    print_ipv4_info(&args->cnx_addr, "Accepted connection from");
    // Spawn new thread to handle the connection
    struct entry *n1 = malloc(sizeof(struct entry));
    int res = pthread_create(&n1->thread_id, NULL, thr_cnx, (void *)args);
    if (res != 0) {
      perror("pthread_create");
      close(args->cnx_fd);
      free(args);
      status = -1;
      goto close_socket;
    }

    SLIST_INSERT_HEAD(&head, n1, entries);
  }

  // Wait for all threads to finish
  while (!SLIST_EMPTY(&head)) { /* List deletion */
    struct entry *n1 = SLIST_FIRST(&head);
    SLIST_REMOVE_HEAD(&head, entries);
    pthread_join(n1->thread_id, NULL);
    free(n1);
  }

  close(output_file);
  remove("/var/tmp/aesdsocketdata");
  status = 0;

close_socket:
  close(sockfd);
    timer_delete(timerid);
  if(output_args != NULL) {
    free(output_args);
  }
  printf("Exiting with status %d\n", status);
  return status;
}