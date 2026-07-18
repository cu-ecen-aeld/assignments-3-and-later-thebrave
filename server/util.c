#include "util.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/syslog.h>

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