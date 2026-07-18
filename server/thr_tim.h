#ifndef _THR_TIM_H_
#define _THR_TIM_H_

#include <pthread.h>
#include <signal.h>

#define SMALL_BUF_SIZE 256

void timer_handler(union sigval sv);
timer_t timer_init(pthread_mutex_t *file_mutex, int output_file, void** output_args);

#endif // _THR_TIM_H_