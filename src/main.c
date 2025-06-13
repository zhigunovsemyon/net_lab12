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

/*
 * Добавить в сервис поддержку дополнительной команды, реализующей телефонный
 * справочник. Клиент отправляет на сервер некоторое символьное имя. Сервер
 * ищет в файле телефон, связанный с этим именем. Если в файле информация не
 * найдена, клиенту возвращается соответствующее сообщение.
	- Входной параметр: ФИО.
	- Ответ сервера: телефон, связанный с введённым ФИО.
 */

// Последняя запись должна содержать null поля
typedef struct {
	const char * name;
	const char * num;
} Entry;

char const * NOT_FOUND_RESPONSE = "No such entry!\n";
constexpr in_port_t PORT = 8789;
[[maybe_unused]] constexpr uint32_t LOCALHOST = (127 << 24) + 1;

int communication_cycle(fd_t fd, Entry const * db);

int main()
{
	Entry const db[] = {
		{"Вася", "7894561122"},
		{"Петя", "1234567788"},
		{"Ваня", "1597534466"},
		{nullptr, nullptr}
	};
	// Структура с адресом и портом клиента
	struct sockaddr_in client_addr = {};
	socklen_t client_addr_len = sizeof(client_addr);

	// Структура с адресом и портом сервера
	struct sockaddr_in server_addr = {};
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(PORT);
	// server_addr.sin_addr = (struct in_addr){htonl(LOCALHOST)};
	server_addr.sin_addr = (struct in_addr){htonl(0)};

	// Входящий сокет
	fd_t serv_sock = create_bind_server_socket(&server_addr);
	if (-1 == serv_sock) {
		perror("Failed to create binded socket");
		return -1;
	}

	printf("Ожидание соединения на порт %hu\n", PORT);
	fd_t client_sock = accept(serv_sock, (struct sockaddr *)&client_addr,
				  &client_addr_len);
	if (client_sock < 0) {
		perror("Accept failed");
		close(serv_sock);
		return 1;
	}
	print_sockaddr_in_info(&client_addr);

	int communication_cycle_bad = communication_cycle(client_sock, db);
	if (communication_cycle_bad < 0) {
		perror("Recv failed");
		close(serv_sock);
		close(client_sock);
		return 1;
	}

	// Штатное завершение работы
	puts("Клиент прервал соединение");
	close(serv_sock);
	close(client_sock);
	// free(db_copy);
	return 0;
}

static inline ssize_t send_bad_request(fd_t fd)
{
	return send(fd, NOT_FOUND_RESPONSE, strlen(NOT_FOUND_RESPONSE), 0);
}

ssize_t handle_request(fd_t fd, Entry const * db, char const * request)
{
	constexpr size_t sendbuf_size = 40;
	char sendbuf[sendbuf_size + 1] = {};

	while (db->name && db->num){
		if (strcmp(db->name,request)){
			db++;
			continue;
		}

		strncpy(sendbuf, db->num, sendbuf_size);
		sendbuf[strlen(sendbuf)] = '\n';
		return send(fd,sendbuf, strlen(sendbuf), 0);
	}

	return send_bad_request(fd);
}

int communication_cycle(fd_t fd, Entry const * db)
{
	constexpr size_t buflen = 64;
	char buf[buflen + 1];
	buf[buflen] = '\0';

	do {
		ssize_t recv_ret = recv(fd, buf, buflen, 0);
		if (recv_ret == 0) {
			break;
		} else if (recv_ret < 0)
			return -1;

		// else if (recv_ret > 0)
		buf[recv_ret] = '\0';

		// Зануление переноса строки
		char * endl = strpbrk(buf, "\r\n");
		if (endl)
			*endl = '\0';

		ssize_t sent_bytes = handle_request(fd, db, buf);
		if (sent_bytes > 0)
			continue;
		else if (sent_bytes == 0)
			break;
		// if error
		return -1;

	} while (true);
	return 0;
}
