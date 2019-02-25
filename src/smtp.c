#include "smtp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <getopt.h>

#include "smtp_sockets.h"
#include "logger.h"


typedef struct smtp_master_thread_state_t {
    int log_socket;
    pid_t log_process;
    
} smtp_master_thread_state_t;


typedef struct smtp_options_t {
    int port;
    int num_threads;
    char *maildir;
    char *dns;
    char *log;
    char *hostname;
    int timeout_secs;
} smtp_options_t;


bool create_log_process(smtp_master_thread_state_t *state, char *filename) {
    
    int sockets[2];
    create_local_socket_pair(sockets);
    
    pid_t pid = fork();
    if (pid == -1) 
    {
        printf("Can't create log process\n");
        return false;
    }
    else if (pid == 0) 
    {
        log_loop(sockets[1], filename);
    }
    else 
    {
        set_log_socket(sockets[0]);
    }
    state->log_socket = sockets[0];
    state->log_process = pid;

    return true;
}


void stop_log_process(smtp_master_thread_state_t *state)
{
    LOG("Stopping log thread");
    send_stop_log();
    int status;
    waitpid(state->log_process, &status, 0); 
}

/**
 * очистка настроек сервера
 */
void clean_options(smtp_options_t *options) 
{
    if (options->maildir) free(options->maildir);
    if (options->dns) free(options->dns);
    if (options->log) free(options->log);
    if (options->hostname) free(options->hostname);
}

// RFC 5321: 4.5.3.2.7. Server Timeout
// По крайней мере 5 минут
static const time_t default_timeout = 60 * 5;

/*
 * загрузка настроек из параметров командной строки для работы сервера
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
                clean_options(options);
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
                clean_options(options);
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
                clean_options(options);
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
    smtp_master_thread_state_t master_thread_state;

    if (!load_smtp_options(argc, argv, &options)) {
        printf("Usage: %s [-p port] [-m maildir] [-l log_file] [-t threads] [-d dns] [-r] [-s timeout_secs] \n", argv[0]);
        return -1;
    }

    printf("Load options success!\n");
    printf("Create log process\n");

    create_log_process(&master_thread_state, options.log);

    LOG("HELO I AM LOGGER");

    stop_log_process(&master_thread_state);

    clean_options(&options);

    return 0;
}