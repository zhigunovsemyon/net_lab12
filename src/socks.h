#ifndef socks_h_
#define socks_h_
#include <netinet/in.h>

// Файловый дескриптор
typedef int fd_t;

fd_t create_command_socket(struct sockaddr_in const * ip_info);

fd_t create_listen_socket(struct sockaddr_in const * ip_info);

void print_sockaddr_in_info(struct sockaddr_in const * addr);

#endif  
