#ifndef _THR_CNX_H_
#define _THR_CNX_H_

#include <pthread.h>
#include <sys/socket.h>

#define SMALL_BUF_SIZE 256

void *thr_cnx(void *arg);

#endif // _THR_CNX_H_