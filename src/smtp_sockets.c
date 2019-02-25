#include "smtp_sockets.h"

#include <sys/types.h>
#include <sys/socket.h>


bool create_local_socket_pair(int *out_sockets)
{
    int result = socketpair(AF_LOCAL, SOCK_DGRAM, 0, out_sockets);
    if (result != 0)
    {
        return false;
    }
    return true;
}