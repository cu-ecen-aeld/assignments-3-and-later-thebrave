#include "thr_cnx.h"

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>
#include <sys/syslog.h>
#include <unistd.h>

#include "thr_cst.h"
#include "util.h"

void *thr_cnx(void *arg) {
  assert(arg != NULL);
  struct thread_args *args = (struct thread_args *)arg;

  // Socket is open, loop until stream ends
  char *buffer = NULL;
  char *tmp_buffer = malloc(SMALL_BUF_SIZE);
  memset(tmp_buffer, 0, SMALL_BUF_SIZE);

  while (true) {
    memset(tmp_buffer, 0, SMALL_BUF_SIZE);
    trace_log(LOG_INFO, "[d] Blocking read");

    // Blocking read from the socket
    ssize_t bytes_read = read(args->cnx_fd, tmp_buffer, SMALL_BUF_SIZE - 1);
    if (bytes_read < 1) {
      trace_log(LOG_INFO, "[d] Connection closed");
      break;
    }

    trace_log(LOG_INFO, "[d] Received %zd bytes: \"%s\"", bytes_read, tmp_buffer);

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

    trace_log(LOG_INFO, "[d] Current buffer %zd bytes: \"%s\"", strlen(buffer),
           buffer);

    // If there is a \n in the string,
    char *pos = strchr(buffer, '\n');
    if (pos != NULL) {
      trace_log(LOG_INFO, "[d] Found a newline");
      ssize_t len = pos - buffer + 1;  // Include newline
      pthread_mutex_lock(args->file_mutex);
      ssize_t bytes_written = write(args->output_file, buffer, len);
      if (bytes_written < 1) {
        perror("write");
        goto inner_cleanup;
      }

      assert(bytes_written == len);
      trace_log(LOG_INFO, "[d] Wrote %zd bytes to file", bytes_written);
      break;
    }
  }

  // Cleanup
  free(buffer);
  buffer = NULL;

  trace_log(LOG_INFO, "[d] Preparing to write to file");
  off_t res = lseek(args->output_file, 0, SEEK_SET);
  if (res == -1) {
    perror("lseek");
    goto inner_cleanup;
  }
  trace_log(LOG_INFO, "[d] Position %zd bytes to file", res);

  // Copy Loop
  ssize_t bytes_read, bytes_written;

  trace_log(LOG_INFO, "[d] Write loop");
  while (true) {
    memset(tmp_buffer, 0, SMALL_BUF_SIZE);

    bytes_read = read(args->output_file, tmp_buffer, SMALL_BUF_SIZE);
    trace_log(LOG_INFO, "[d] Read %zd bytes", bytes_read);
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
      bytes_written = write(args->cnx_fd, tmp_buffer + total_written,
                            bytes_read - total_written);
      trace_log(LOG_INFO, "[d] Wrote %zd/%zd bytes", bytes_written, total_written);

      if (bytes_written < 0) {
        // If interrupted by signal, retry; otherwise handle error
        if (errno == EINTR) continue;
        goto inner_cleanup;
      }
      total_written += bytes_written;
    }
  }

inner_cleanup:
  pthread_mutex_unlock(args->file_mutex);
  free(tmp_buffer);
  // Close connexion
  close(args->cnx_fd);

  trace_log(LOG_INFO, "[d] Done writing");
  print_ipv4_info(&args->cnx_addr, "Closed connection from");
  free(args);

  return NULL;
}