#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>


void get_random(size_t size, char *buf)
{
     int urandom = open("/dev/urandom", O_RDONLY);
     read(urandom, buf, size);
}