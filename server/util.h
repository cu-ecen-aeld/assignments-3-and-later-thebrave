#ifndef _UTIL_H_
#define _UTIL_H_

#include <sys/socket.h>
#include <sys/syslog.h>

void print_ipv4_info(struct sockaddr *addr, const char *prefix);
void trace_log(int priority, const char *format, ...);

#define syslog trace_log

#endif // _UTIL_H_