#include "socks.h"
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>

int get_local_ip(char * ip_buffer, size_t buffer_size)
{
	int sock;
	struct sockaddr_in server_addr;
	struct sockaddr_in local_addr;
	socklen_t addr_len = sizeof(local_addr);

	// Создать UDP-сокет
	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		perror("Не удалось создать сокет");
		return -1;
	}

	// Настроить адрес внешнего сервера (например, Google DNS)
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(80); // Любой порт, например, 80
	inet_pton(
		AF_INET, "8.8.8.8",
		&server_addr.sin_addr); // Внешний IP (не подключаемся реально)

	// Выполнить "подключение" (не отправляет данные, только выбирает
	// интерфейс)
	if (connect(sock, (struct sockaddr *)&server_addr,
		    sizeof(server_addr)) < 0) {
		perror("Ошибка connect");
		close(sock);
		return -1;
	}

	// Получить локальный IP-адрес, связанный с сокетом
	if (getsockname(sock, (struct sockaddr *)&local_addr, &addr_len) < 0) {
		perror("Ошибка getsockname");
		close(sock);
		return -1;
	}

	// Конвертировать IP-адрес в строку
	if (inet_ntop(AF_INET, &local_addr.sin_addr, ip_buffer, buffer_size) ==
	    NULL) {
		perror("Ошибка inet_ntop");
		close(sock);
		return -1;
	}

	// Закрыть сокет
	close(sock);
	return 0;
}

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

fd_t create_command_socket(struct sockaddr_in const * ip_info)
{
	// Входящий сокет
	fd_t sock = socket(AF_INET, SOCK_STREAM, 6);
	if (sock == -1)
		return -1;

	// Соединение сокета с портом и адресом
	if (connect(sock, (struct sockaddr const *)ip_info, sizeof(*ip_info))) {
		close(sock);
		return -1;
	}

	return sock;
}

fd_t create_listen_socket(struct sockaddr_in const * ip_info)
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
