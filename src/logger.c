#include "logger.h"
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <fcntl.h>
#include <stdarg.h>

#include "dynamic_vector.h"

static int G_logsocket;

void set_log_socket(int socket)
{
    G_logsocket = socket;
}

static char *write_message(const char *file, int line, const char *format, va_list args, va_list args2)
{   
    char timebuf[20];
    time_t now = time(NULL);
    strftime(timebuf, 20, "%Y-%m-%d %H:%M:%S", localtime(&now));
    
    int len = vsnprintf(NULL, 0, format, args);
    char *buff = malloc(len + 1);
    vsnprintf(buff, len + 1, format, args2);
    
    char name[16];
    pthread_getname_np(pthread_self(), name, 16);
    
    len = snprintf(NULL, 0, "[%s] %s %s:%d %s\n", name, timebuf, file, line, buff);
    char *final_buff = malloc(len + 1);
    snprintf(final_buff, len + 1, "[%s] %s %s:%d %s\n", name, timebuf, file, line, buff);
    
    free(buff);
    return final_buff;
}

void write_to_log(const char *file, int line, const char *format, ...)
{
    va_list args, args2;
    va_start(args, format);
    va_start(args2, format);
    char *buff = write_message(file, line, format, args, args2);
    va_end(args2);
    va_end(args);

    int l = strlen(buff);
    write(G_logsocket, &l, sizeof(int));
    write(G_logsocket, buff, l);
    free(buff);
}

void send_stop_log()
{
    int stopSignal = -1;
    write(G_logsocket, &stopSignal, sizeof(int));
    close(G_logsocket);
}


void log_loop(int socket, char *filename)
{
    dynamic_vector_t buffer = dynamic_vector_create(sizeof(char), 256);
    int logfile = open(filename, O_CREAT | O_WRONLY | O_APPEND, 0644);
    while(1)
    {      
        int len = 0;
        int read_bytes = read(socket, &len, sizeof(int));
        if (read_bytes == 0 || len <= 0)
        {
            break;
        }

        if (buffer.capacity < len)
        {
            dynamic_vector_reserve_at_least(&buffer, len);
        }

        read_bytes = read(socket, buffer.data, len);
        if (read_bytes != len)
        {
            const char *error_str = "LOG: COULD NOT READ FROM SOCKET\n";
            write(logfile, error_str, strlen(error_str));
            break;
        }
        
        write(logfile, buffer.data, len);
    }
    
    close(logfile);
    dynamic_vector_close(&buffer);
    exit(0);
}
