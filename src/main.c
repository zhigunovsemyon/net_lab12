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

[[maybe_unused]] constexpr uint32_t LOCALHOST = (127 << 24) + 1;

int communication_cycle(fd_t fd);

int login(fd_t fd);

int main()
{
	// Структура с адресом и портом сервера
	struct sockaddr_in server_addr = {};
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(21);
	server_addr.sin_addr = (struct in_addr){htonl(LOCALHOST)};
	// server_addr.sin_addr = (struct in_addr){htonl(0)};

	// Входящий сокет
	fd_t cmd_sock = create_connected_socket(&server_addr);
	if (-1 == cmd_sock) {
		perror("Failed to create command socket");
		return -1;
	}

	int login_bad = login(cmd_sock);
	if (login_bad == -1) {
		perror("login failed");
		close(cmd_sock);
		return 1;
	} else if (login_bad == -2) {
		fprintf(stderr, "Неверный пароль\n");
		close(cmd_sock);
		return 1;
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

int set_ip_from_pasv_responce(char const * str,
			      struct sockaddr_in * addr_to_set)
{
	// 1-4 октеты
	uint8_t fsto, sndo, trdo, ftho;
	// Половинки портов
	uint8_t port_mult, port_add;

	if (6 != sscanf(str, "(%hhu,%hhu,%hhu,%hhu,%hhu,%hhu)", &fsto, &sndo,
			&trdo, &ftho, &port_mult, &port_add)) {
		return -1;
	}

	addr_to_set->sin_port = htons(port_add + 256 * port_mult);
	addr_to_set->sin_addr.s_addr = htonl((uint32_t)(fsto << 24));
	addr_to_set->sin_addr.s_addr += htonl((uint32_t)(sndo << 16));
	addr_to_set->sin_addr.s_addr += htonl((uint32_t)(trdo << 8));
	addr_to_set->sin_addr.s_addr += htonl((uint32_t)ftho);

	return 0;
}

fd_t set_pasv_connection(fd_t cmd_sock)
{
	// Структура с адресом и портом сервера
	struct sockaddr_in server_addr = {};
	server_addr.sin_family = AF_INET;

	if (send(cmd_sock, "PASV\n", strlen("PASV\n"), 0) < 0)
		return -1;

	char response[64];
	if (recv(cmd_sock, response, sizeof(response), 0) < 0)
		return -1;

	if (strncmp(response, "227 ", 4)) // 227 -- код перехода в пас.режим
		return -1;

	char const * ip_port_str = strchr(response, '(');
	if (!ip_port_str)
		return -1;

	if (set_ip_from_pasv_responce(ip_port_str, &server_addr))
		return -1;

	// -1 по ошибке, либо нормальный сокет
	return create_connected_socket(&server_addr);
}

int quit(fd_t cmd_sock)
{
	if (send(cmd_sock, "quit\n", strlen("quit\n"), 0) < 0)
		return -1;

	ssize_t recieved;
	constexpr ssize_t response_buf_len = 100;
	char response[response_buf_len + 1];
	response[response_buf_len] = '\0';

	recieved = recv(cmd_sock, response, response_buf_len, 0);
	if (recieved < 0)
		return -1;
	response[recieved] = '\0';

	// 221 -- код выхода по quit
	if (strncmp(response, "221 ", 4)) {
		fprintf(stderr, "Ошибка: %s\n", response);
		return 0;
	}
	return 0;
}

int list(fd_t cmd_sock, fd_t conn_sock)
{
	if (send(cmd_sock, "LIST\n", strlen("LIST\n"), 0) < 0)
		return -1;

	ssize_t recieved;
	constexpr ssize_t response_buf_len = 100;
	char response[response_buf_len + 1];
	response[response_buf_len] = '\0';

	recieved = recv(cmd_sock, response, response_buf_len, 0);
	if (recieved < 0)
		return -1;
	response[recieved] = '\0';

	// 150 -- код подготовки обмена
	if (strncmp(response, "150 ", 4)) {
		fprintf(stderr, "Ошибка: %s\n", response);
		return 0;
	}

	do {
		recieved = recv(conn_sock, response, response_buf_len, 0);
		if (recieved < 0)
			return -1;
		response[recieved] = '\0';
		printf("%s", response);

	} while (recieved == response_buf_len);

	// Получение статуса отправки
	recieved = recv(cmd_sock, response, response_buf_len, 0);
	if (recieved < 0)
		return -1;
	response[recieved] = '\0';
	// 226 -- код успешного обмена
	if (strncmp(response, "226 ", 4)) {
		fprintf(stderr, "Ошибка: %s\n", response);
		return 0;
	}
	return 0;
}

int communication_cycle(fd_t cmd_sock)
{
	printf("in cycle\n");
	for (;;) {
		fd_t com_sock = set_pasv_connection(cmd_sock);
		if (-1 == com_sock) {
			perror("Не удалось создать сокет пассивного режима");
			return -1;
		}

		int cmd = getchar();
		switch (cmd) {
		case 'q':
		case 'Q':
			while (getchar() != '\n')
				;
			// падение в eof
		case EOF:
			if (quit(cmd_sock) < 0) {
				perror("quit failed");
				close(com_sock);
				return -1;
			}
			close(com_sock);
			return 0;
		case 'c':
		case 'C':
			while (getchar() != '\n')
				;
			printf("create\n");
			break;
		case 'd':
		case 'D':
			while (getchar() != '\n')
				;
			printf("delete\n");
			break;
		case 'l':
		case 'L':
			while (getchar() != '\n')
				;
			printf("list\n");
			if (list(cmd_sock, com_sock) < 0) {
				perror("list failed");
				close(com_sock);
				return -1;
			}
			break;
		default:
			while (getchar() != '\n')
				;
			fprintf(stderr, "Неизвестная команда\n");
		}

		close(com_sock);
	}
}

int login(fd_t cmd_sock)
{
	constexpr size_t buflen = 48;
	char buf[buflen + 1];
	buf[buflen] = 0;
	ssize_t recieved;

	// Получение заголовка
	recieved = recv(cmd_sock, buf, buflen, 0);
	if (recieved < 0) {
		perror("recv failed");
		return -1;
	}
	buf[recieved] = '\0';
	printf("Подключено к:%s", strchr(buf, ' '));

	constexpr int user_maxstrlen = 100;
	char user[user_maxstrlen] = "USER ";
	printf("Пользователь: ");
	// Ввод за словом USER
	if (!fgets(user + strlen(user), user_maxstrlen - (int)strlen(user),
		   stdin))
		return -2;

	// Отправка логина
	if (send(cmd_sock, user, strlen(user), 0) < 0) {
		perror("send failed");
		return -1;
	}
	// Получение информации об ожидании пароля
	recieved = recv(cmd_sock, buf, buflen, 0);
	if (recieved < 0) {
		perror("recv failed");
		return -1;
	}
	buf[recieved] = '\0';

	bool not_asking_for_pass = strncmp(buf, "331", 3);
	if (not_asking_for_pass)
		return -1;

	char pass[101] = "PASS ";
	pass[100] = '\0';
	char * tmp_pass = getpass("Пароль: "); // не оставляет \n в stdin

	ssize_t space_in_pass = (ssize_t)sizeof(pass) - (ssize_t)strlen(pass);
	assert(space_in_pass > 0);
	strncat(pass, tmp_pass, (size_t)space_in_pass);
	strcat(pass, "\n");
	free(tmp_pass);

	// Отправление пароля
	if (send(cmd_sock, pass, strlen(pass), 0) < 0) {
		perror("send failed");
		return -1;
	}

	recieved = recv(cmd_sock, buf, buflen, 0);
	if (recieved < 0) {
		perror("recv failed");
		return -1;
	}
	buf[recieved] = '\0';

	// 230 -- successful login
	if (!strncmp("230", buf, 3))
		return 0;

	return -2;
}
