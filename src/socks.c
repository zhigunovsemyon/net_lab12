#include "socks.h"
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>

void print_sockaddr_in_info(struct sockaddr_in const * addr)
{
	// 1-4 октеты
	uint8_t const fsto = ntohl(addr->sin_addr.s_addr) >> 24;
	uint8_t const sndo = 0xFF & (ntohl(addr->sin_addr.s_addr) >> 16);
	uint8_t const trdo = 0xFF & (ntohl(addr->sin_addr.s_addr) >> 8);
	uint8_t const ftho = 0xFF & (ntohl(addr->sin_addr.s_addr));

	printf("Подключение от:%hhu.%hhu.%hhu.%hhu", fsto, sndo, trdo, ftho);
	printf(":%hu\n", ntohs(addr->sin_port));
}

fd_t create_bind_server_socket(struct sockaddr_in const * ip_info)
{
	// Входящий сокет
	fd_t serv_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (serv_sock == -1)
		return -1;

	// Соединение сокета с портом и адресом
	if (bind(serv_sock, (struct sockaddr const *)ip_info,
		 sizeof(*ip_info))) {
		close(serv_sock);
		return -1;
	}

	// Создание очереди запросов на соединение
	if (listen(serv_sock, 100)) {
		close(serv_sock);
		return -1;
	}

	return serv_sock;
}
