#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <getopt.h>

#include "smtp.h"

typedef struct smtp_options_t {
    int port;
    int num_threads;
    char *maildir;
    char *dns;
    char *log;
    char *hostname;
    int timeout_secs;
} smtp_options_t;

// RFC 5321: 4.5.3.2.7. Server Timeout
// По крайней мере 5 минут
static const time_t default_timeout = 60 * 5;

/*
 * загрузка настроек из парамтров командной строки для работы сервера
 */
bool load_smtp_options(int argc, char *argv[], smtp_options_t *options){
    options->port = 8080;
    options->num_threads = 1;
    options->maildir = NULL;
    options->dns = NULL;
    options->log = NULL;
    options->hostname = NULL;
    options->timeout_secs = default_timeout;
    
    int opt;
    while((opt = getopt(argc, argv, "t:m:p:d:l:n:s:")) != -1){
        switch(opt)
        {
        case 't':
            options->num_threads = atoi(optarg);
            if (options->num_threads < 1)
            {
                if (options->maildir)
                {
                    free(options->maildir);
                }
                return false;
            }
            break;
        case 'm':
            options->maildir = strdup(optarg);
            break;
        case 'p':
            options->port = atoi(optarg);
            if (options->port < 1)
            {
                free(options->maildir);
                free(options->log);
                free(options->dns);
                return false;
            }
            break;
        case 'd':
            options->dns = strdup(optarg);
            break;
        case 'l':
            options->log = strdup(optarg);
            break;
        case 'n':
            options->hostname = strdup(optarg);
            break;
        case 's':
            options->timeout_secs = atoi(optarg);
            if (options->timeout_secs < 1)
            {
                free(options->maildir);
                free(options->log);
                free(options->dns);
                return false;
            }
            break;
        default:
            break;
        }
    }

    if (!options->maildir)
    {
        options->maildir = strdup("./maildir/");
    }

    if (!options->log)
    {
        options->log = strdup("./smtp_log.txt");
    }

    return true;
}


int smtp(int argc, char *argv[]){
    smtp_options_t options;

    if (!load_smtp_options(argc, argv, &options)) {
        printf("Usage: %s [-p port] [-m maildir] [-l log_file] [-t threads] [-d dns] [-r] [-s timeout_secs] \n", argv[0]);
        return -1;
    }
    printf("Load options success!\n");

    return 0;
}