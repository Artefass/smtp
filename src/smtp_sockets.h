#pragma once
#include <stdbool.h>

/**
 * @file
 * @brief Операции над сокетами
 */

void set_socket_nonblocking(int socket, bool is_nonblocking);
bool create_server_socket_ipv4(char *host, int port, int *out_socket);
bool create_server_socket_ipv6(char *host, int port, int *out_socket);
int create_local_socket_pair(int *out_sockets);
void do_listen(int socket, int backlog);
int send_to_socket(int socket, char *buf, int len);
int read_from_socket(int socket, char *buf, int len);
char *get_hostname();
