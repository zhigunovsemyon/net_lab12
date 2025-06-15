#ifndef FTP_H_
#define FTP_H_
#include "socks.h"

int set_destination(struct sockaddr_in * addr_to_set);

int communication_cycle(fd_t fd);

int login(fd_t fd);

#endif // !FTP_H_
