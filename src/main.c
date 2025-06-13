#include "socks.h"
#include <assert.h>
#include <malloc.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

/* 	Необходимо разработать простой FTP-клиент. Для всех вариантов
 * разработанный клиент должен иметь возможность выводить список файлов и
 * папок корневой директории сервера (поддержка команды LIST).
	Клиент должен использовать активный режим
	Дополнительно к базовой функциональности необходимо выполнить
 * индивидуальное задание: Создание и удаление каталогов на удаленном сервере.
 */

char const * NOT_FOUND_RESPONSE = "No such entry!\n";
[[maybe_unused]] constexpr in_port_t LISTEN_PORT = 18789;
[[maybe_unused]] constexpr uint32_t LOCALHOST = (127 << 24) + 1;

int communication_cycle(fd_t fd);

int main()
{
	// Структура с адресом и портом сервера
	struct sockaddr_in server_addr = {};
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(21);
	server_addr.sin_addr = (struct in_addr){htonl(LOCALHOST)};
	// server_addr.sin_addr = (struct in_addr){htonl(0)};

	// Входящий сокет
	fd_t cmd_sock = create_command_socket(&server_addr);
	if (-1 == cmd_sock) {
		perror("Failed to create command socket");
		return -1;
	}

	int communication_cycle_bad = communication_cycle(cmd_sock);
	if (communication_cycle_bad < 0) {
		perror("cycle failed");
		close(cmd_sock);
		return 1;
	}

	// Штатное завершение работы
	puts("Клиент прервал соединение");
	close(cmd_sock);
	return 0;
}

[[maybe_unused]] static ssize_t send_port(fd_t cmd_sock,
					  struct sockaddr_in const * addr)
{
	char buf[26];
	// 1-4 октеты
	uint8_t const fsto = ntohl(addr->sin_addr.s_addr) >> 24;
	uint8_t const sndo = 0xFF & (ntohl(addr->sin_addr.s_addr) >> 16);
	uint8_t const trdo = 0xFF & (ntohl(addr->sin_addr.s_addr) >> 8);
	uint8_t const ftho = 0xFF & (ntohl(addr->sin_addr.s_addr));

	uint16_t const port = ntohs(addr->sin_port);

	sprintf(buf, "%hhu,%hhu,%hhu,%hhu,%hhu,%hhu\n", fsto, sndo, trdo, ftho,
		port / 256, port % 256);
	return send(cmd_sock, buf, strlen(buf), 0);
}

int communication_cycle(fd_t cmd_sock)
{
	int rc = 0;
	constexpr size_t buflen = 64;
	char buf[buflen + 1];
	buf[buflen] = '\0';
	buf[0] = '\0';
	do {
		ssize_t read_from = recv(cmd_sock, buf, buflen, 0);
		if (read_from < 0) {
			perror("send failed");
			goto cycle_end;
		}
		buf[read_from] = '\0';
		printf("%64s", buf);

		if(!fgets(buf,buflen, stdin))
			goto cycle_end;

		if (send(cmd_sock, buf, strlen(buf), 0) < 0) {
			perror("send failed");
			rc = -1;
			goto cycle_end;
		}

	} while (true);

cycle_end:
	return rc;
}
