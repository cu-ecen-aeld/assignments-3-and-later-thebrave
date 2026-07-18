#ifndef _THR_CST_H_
#define _THR_CST_H_

#include <sys/socket.h>

struct thread_args {
  // Incoming connection
  int cnx_fd;
  struct sockaddr cnx_addr;
  socklen_t cnx_addrlen;

  // Output file descriptor & sync primitive
  int output_file;
  pthread_mutex_t *file_mutex;
};

#endif // _THR_CST_H_