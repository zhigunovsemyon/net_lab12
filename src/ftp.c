#include "ftp.h"
#include <arpa/inet.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

constexpr in_port_t DEFAULT_PORT = 21;

int set_destination(struct sockaddr_in * addr_to_set)
{
	constexpr size_t dest_str_size = 24;
	char dest_str[dest_str_size + 1];
	printf("IPv4-адрес[:порт]: ");
	if (!fgets(dest_str, dest_str_size, stdin))
		return -1;

	memset(addr_to_set, 0, sizeof(*addr_to_set));
	addr_to_set->sin_family = AF_INET;

	char * port_str = strchr(dest_str, ':');
	// Если порт не указан
	if (!port_str) {
		// Установка стандартного порта
		addr_to_set->sin_port = htons(DEFAULT_PORT);
	} else {
		// Непосредственно установка порта
		int new_port = atoi(1 + port_str);
		addr_to_set->sin_port = (new_port < 1 || new_port > UINT16_MAX)
						? htons(DEFAULT_PORT)
						: htons((in_port_t)new_port);

		// При установке IP :порт считываться не будет
		*port_str = '\0';
	}

	// Установка IP адреса
	addr_to_set->sin_addr.s_addr = inet_addr(dest_str);
	if (addr_to_set->sin_addr.s_addr == (in_addr_t)-1)
		return -1;

	return 0;
}

static int set_ip_from_pasv_responce(char const * str,
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

static fd_t set_pasv_connection(fd_t cmd_sock)
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

// -1 при ошибке передачи, 1 при EOF, 0 при успехе
static int rm_dir(fd_t cmd_sock)
{
	constexpr size_t dirname_bufsize = 100;
	char dirname[dirname_bufsize + 1];
	printf("Каталог: ");
	if (!fgets(dirname, dirname_bufsize, stdin))
		return 1;

	char * endl = strchr(dirname, '\n');
	if (!endl) {
		ssize_t buf_space =
			(ssize_t)dirname_bufsize - (ssize_t)strlen(dirname) - 1;
		assert(buf_space >= 0);
		if (!buf_space) {
			fprintf(stderr, "Слишком длинное имя каталога!\n");
			return 0;
		}
		strcat(dirname, "\n");
	}

	if (send(cmd_sock, "RMD ", strlen("RMD "), 0) < 0)
		return -1;
	if (send(cmd_sock, dirname, strlen(dirname), 0) < 0)
		return -1;

	ssize_t recieved;
	constexpr ssize_t response_buf_len = 100;
	char response[response_buf_len + 1];
	response[response_buf_len] = '\0';

	recieved = recv(cmd_sock, response, response_buf_len, 0);
	if (recieved < 0)
		return -1;
	response[recieved] = '\0';

	// 250 -- код удаления каталога
	if (strncmp(response, "250 ", 4)) {
		fprintf(stderr, "Ошибка: %s\n", response);
		return 0;
	}
	return 0;
}

// -1 при ошибке передачи, 1 при EOF, 0 при успехе
static int create_dir(fd_t cmd_sock)
{
	constexpr size_t dirname_bufsize = 100;
	char dirname[dirname_bufsize + 1];
	printf("Каталог: ");
	if (!fgets(dirname, dirname_bufsize, stdin))
		return 1;

	char * endl = strchr(dirname, '\n');
	if (!endl) {
		ssize_t buf_space =
			(ssize_t)dirname_bufsize - (ssize_t)strlen(dirname) - 1;
		assert(buf_space >= 0);
		if (!buf_space) {
			fprintf(stderr, "Слишком длинное имя каталога!\n");
			return 0;
		}
		strcat(dirname, "\n");
	}

	if (send(cmd_sock, "MKD ", strlen("MKD "), 0) < 0)
		return -1;
	if (send(cmd_sock, dirname, strlen(dirname), 0) < 0)
		return -1;

	ssize_t recieved;
	constexpr ssize_t response_buf_len = 100;
	char response[response_buf_len + 1];
	response[response_buf_len] = '\0';

	recieved = recv(cmd_sock, response, response_buf_len, 0);
	if (recieved < 0)
		return -1;
	response[recieved] = '\0';

	// 257 -- код создания каталога
	if (strncmp(response, "257 ", 4)) {
		fprintf(stderr, "Ошибка: %s\n", response);
		return 0;
	}
	return 0;
}

static int quit(fd_t cmd_sock)
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

static int list(fd_t cmd_sock)
{
	fd_t conn_sock = set_pasv_connection(cmd_sock);
	if (-1 == conn_sock) {
		perror("Не удалось создать сокет пассивного режима");
		return -1;
	}

	if (send(cmd_sock, "LIST\n", strlen("LIST\n"), 0) < 0) {
		close(conn_sock);
		return -1;
	}

	ssize_t recieved;
	constexpr ssize_t response_buf_len = 100;
	char response[response_buf_len + 1];
	response[response_buf_len] = '\0';

	recieved = recv(cmd_sock, response, response_buf_len, 0);
	if (recieved < 0) {
		close(conn_sock);
		return -1;
	}
	response[recieved] = '\0';

	// 150 -- код подготовки обмена
	if (strncmp(response, "150 ", 4)) {
		fprintf(stderr, "Ошибка: %s\n", response);
		close(conn_sock);
		return 0;
	}

	do {
		recieved = recv(conn_sock, response, response_buf_len, 0);
		if (recieved < 0) {
			close(conn_sock);
			return -1;
		}
		response[recieved] = '\0';
		printf("%s", response);

	} while (recieved == response_buf_len);

	// Получение статуса отправки
	recieved = recv(cmd_sock, response, response_buf_len, 0);
	if (recieved < 0) {
		close(conn_sock);
		return -1;
	}
	response[recieved] = '\0';

	// 226 -- код успешного обмена
	if (strncmp(response, "226 ", 4))
		fprintf(stderr, "Ошибка: %s\n", response);

	close(conn_sock);
	return 0;
}

int communication_cycle(fd_t cmd_sock)
{
	int rc;
	for (;;) {
		printf("Введите одну из комманд:\n"
		       "l -- список файлов\n"
		       "d -- удалить папку\n"
		       "с -- создать папку\n"
		       "q/C-d -- завершение соединения\n"
		       "-> ");
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
				return -1;
			}
			return 0;
		case 'c':
		case 'C':
			while (getchar() != '\n')
				;
			rc = create_dir(cmd_sock);
			if (rc < 0) {
				perror("create_dir failed");
				return -1;
			}
			if (rc > 0) { // EOF
				if (quit(cmd_sock) < 0) {
					perror("quit failed");
					return -1;
				}
				return 0;
			}
			break;
		case 'd':
		case 'D':
			while (getchar() != '\n')
				;
			rc = rm_dir(cmd_sock);
			if (rc < 0) {
				perror("rm_dir failed");
				return -1;
			}
			if (rc > 0) { // EOF
				if (quit(cmd_sock) < 0) {
					perror("quit failed");
					return -1;
				}
				return 0;
			}
			break;
		case 'l':
		case 'L':
			while (getchar() != '\n')
				;
			if (list(cmd_sock) < 0) {
				perror("list failed");
				return -1;
			}
			break;
		default:
			while (getchar() != '\n')
				;
			fprintf(stderr, "Неизвестная команда\n");
		}
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
