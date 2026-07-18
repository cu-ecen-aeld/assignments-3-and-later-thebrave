#ifndef _UTIL_H_
#define _UTIL_H_

#include <sys/socket.h>

void print_ipv4_info(struct sockaddr *addr, const char *prefix);

#endif // _UTIL_H_