#include "thr_tim.h"

#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syslog.h>
#include <unistd.h>

#include "thr_cst.h"

void timer_handler(union sigval sv) {
  char timestamp[SMALL_BUF_SIZE];

  assert(sv.sival_ptr != NULL);
  struct thread_args *args = (struct thread_args *)sv.sival_ptr;
  assert(args->file_mutex != NULL);
  assert(args->output_file != 0);

  pthread_mutex_lock(args->file_mutex);
  syslog(LOG_INFO, "Timer triggered, writing to file");
  // Write the current timestamp to the output file
  time_t now = time(NULL);
  struct tm *tm = localtime(&now);

  if (0 != strftime(timestamp, SMALL_BUF_SIZE, "timestamp:%a, %d %b %Y %T %z\n", tm)) {
    write(args->output_file, timestamp, strlen(timestamp));
  }

  pthread_mutex_unlock(args->file_mutex);
}

timer_t timer_init(pthread_mutex_t *file_mutex, int output_file, void** output_args) {
  struct thread_args *args = malloc(sizeof(struct thread_args));
  *output_args = args;
  memset(args, 0, sizeof(struct thread_args));
  args->file_mutex = file_mutex;
  args->output_file = output_file;

  timer_t timerid;
  struct sigevent sevp;
  memset(&sevp, 0, sizeof(struct sigevent));
  sevp.sigev_notify = SIGEV_THREAD;
  sevp.sigev_notify_function = timer_handler;
  sevp.sigev_value.sival_ptr = args;
  if (-1 == timer_create(CLOCK_MONOTONIC, &sevp, &timerid)) {
    perror("timer_create");
    free(args);
    return 0;
  }

  printf("timer ID is %#jx\n", (uintmax_t)timerid);

  /* Start the timer.  */
  struct itimerspec its;
  its.it_value.tv_sec = 10;
  its.it_value.tv_nsec = 0;
  its.it_interval.tv_sec = its.it_value.tv_sec;
  its.it_interval.tv_nsec = its.it_value.tv_nsec;

  if (timer_settime(timerid, 0, &its, NULL) == -1) {
    perror("timer_settime");
    free(args);
    return 0;
  }

  return timerid;
}