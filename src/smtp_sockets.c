#include "smtp_sockets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include "logger.h"


void set_socket_nonblocking(int socket, bool is_nonblocking)
{
    int flags = fcntl(socket, F_GETFL);
    if (is_nonblocking) fcntl(socket, F_SETFL, flags | O_NONBLOCK);
    else fcntl(socket, F_SETFL, flags & ~O_NONBLOCK);
}

bool reuse_addr(int sock, bool enable)
{
    int option = (int)enable;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(int)) < 0)
    {
        LOG("Error reuse addr %d", errno);
        return false;
    }

    return true;
}

bool v6_only(int sock, bool enable)
{
    int option = (int)enable;
    if (setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, &option, sizeof(int)) < 0)
    {
        LOG("Error setting v6 only %d", errno);
        return false;
    }
    
    return true;
}

bool create_server_socket_ipv4(char *host, int port, int *out_socket)
{
    int result = socket(AF_INET, SOCK_STREAM, 0);
    if (result < 0)
    {
        LOG("Error creating v4 server socket: %d", result);
        return false;
    }

    struct sockaddr_in server_address = {};
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    server_address.sin_addr.s_addr = INADDR_ANY;

    int error = inet_aton(host, &server_address.sin_addr);
    if (error == 0)
    {
        LOG("Could not parse v4 host");
        return false;
    }

    if (!reuse_addr(result, true))
    {
        return false;
    }

    error = bind(result, (struct sockaddr*)&server_address, sizeof(server_address));
    if (error != 0)
    {
        LOG("Error binding v4: %d", errno);
        return false;
    }

    *out_socket = result;
    return true;    
}

bool create_server_socket_ipv6(char *host, int port, int *out_socket)
{
    int result = socket(AF_INET6, SOCK_STREAM, 0);
    if (result < 0)
    {
        LOG("Error creating v6 server socket: %d", result);
        return false;
    }

    const struct in6_addr in6addr_any = IN6ADDR_ANY_INIT;

    struct sockaddr_in6 server_address = {};
    server_address.sin6_family = AF_INET6;
    server_address.sin6_port = htons(port);
    server_address.sin6_addr = in6addr_any;

    int error = inet_pton(AF_INET6, host, &server_address.sin6_addr);
    if (error == 0)
    {
        LOG("Could not parse v6 host\n");
        return false;
    }

    if (!reuse_addr(result, true))
    {
        LOG("Could not reuse v6 addr");
        return false;
    }

    if (!v6_only(result, true))
    {
        LOG("Could not set v6 only");
        return false;
    }

    error = bind(result, (struct sockaddr*)&server_address, sizeof(server_address));
    if (error != 0)
    {
        LOG("Error binding v6: %d", errno);
        return false;
    }

    *out_socket = result;
    return true;    
}

void do_listen(int socket, int backlog)
{
    listen(socket, backlog);
}

char *get_hostname()
{
    struct addrinfo hints, *info;
    int gai_result;

    
    char namebuf[1024];
    namebuf[1023] = '\0';
    gethostname(namebuf, 1024);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; /*either IPV4 or IPV6*/
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_CANONNAME;

    if ((gai_result = getaddrinfo(namebuf, "http", &hints, &info)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(gai_result));
        return false;
    }
    
    char *name = strdup(info->ai_canonname);
    freeaddrinfo(info);
    
    return name;
/*
    for(p = info; p != NULL; p = p->ai_next) {
        printf("hostname: %s\n", p->ai_canonname);
    }

    freeaddrinfo(info);
*/
}

int create_local_socket_pair(int *out_sockets)
{
    int result = socketpair(AF_LOCAL, SOCK_DGRAM, 0, out_sockets);
    if (result != 0)
    {
        LOG("Error creating socket pair: %d", result);
        return false;
    }
    return true;
}

int send_to_socket(int socket, char *buf, int len)
{
    int err = send(socket, buf, len, 0);
    if (err >= 0) return err;
    
    if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) return 0;    
    return -errno;
}

int read_from_socket(int socket, char *buf, int len)
{
    int err = recv(socket, buf, len, 0);
    if (err >= 0) return err;
    
    if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) return 0;    
    return -errno;
}

