#include "smtp.h"

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <pthread.h>
#include <assert.h>
#include <unistd.h>
#include <limits.h>
#include <signal.h>
#include <firedns.h>

#include "commands.h"
#include "maildir.h"
#include "dynamic_vector.h"
#include "smtp_sockets.h"
#include "smtp_worker.h"
#include "logger.h"


typedef struct smtp_master_thread_state_t
{
    int listen_socket;
    int listen_socket_v6;
    maildir_t maildir;
    int num_threads;
    smtp_worker_thread_state_t *threads;
    int log_socket;
    pid_t log_process;
    char *dns;
    char *hostname;
    bool random_filenames;
    int timeout_secs;
} smtp_master_thread_state_t;

typedef struct smtp_options_t
{
    int port;
    int num_threads;
    char *maildir;
    char *dns;
    char *log;
    char *hostname;
    bool random_filenames;
    int timeout_secs;
} smtp_options_t;


// RFC 5321: 4.5.3.2.7. Server Timeout
// По крайней мере 5 минут
static const time_t default_timeout = 60 * 5;

bool parse_options(int argc, char **argv, smtp_options_t *result)
{
    smtp_options_t options = {};
    options.maildir = NULL;
    options.dns = NULL;
    options.log = NULL;
    options.num_threads = 1;
    options.port = 8080;
    options.random_filenames = true;
    options.timeout_secs = default_timeout;

    int opt;
    while ((opt = getopt(argc, argv, "t:m:p:d:rl:n:s:")) != -1)
    {
        switch(opt)
        {
        case 't':
            options.num_threads = atoi(optarg);
            if (options.num_threads < 1)
            {
                if (options.maildir)
                {
                    free(options.maildir);
                }
                return false;
            }
            break;
        case 'm':
            options.maildir = strdup(optarg);
            break;
        case 'p':
            options.port = atoi(optarg);
            if (options.port < 1)
            {
                free(options.maildir);
                free(options.log);
                free(options.dns);
                return false;
            }
            break;
        case 'd':
            options.dns = strdup(optarg);
            break;
        case 'r':
            options.random_filenames = false;
            break;
        case 'l':
            options.log = strdup(optarg);
            break;
        case 'n':
            options.hostname = strdup(optarg);
            break;
        case 's':
            options.timeout_secs = atoi(optarg);
            if (options.timeout_secs < 1)
            {
                free(options.maildir);
                free(options.log);
                free(options.dns);
                return false;
            }
            break;
        default:
            break;
        }
    }

    if (!options.maildir)
    {
        options.maildir = strdup("./maildir/");
    }

    if (!options.log)
    {
        options.log = strdup("./smtp_log.txt");
    }

    if (!options.hostname)
    {
        options.hostname = get_hostname();
    }

    *result = options;
    return true;
}

void *worker_thread_fn(void *arg)
{
    smtp_worker_thread_state_t *state = (smtp_worker_thread_state_t*)arg;
    worker_loop(state);
    return NULL;
}

bool spawn_worker_thread(int id, smtp_master_thread_state_t *master_state, smtp_worker_thread_state_t *state)
{
    int sockets[2];
    if (!create_local_socket_pair(sockets))
    {
        return false;
    }

    state->id = id;
    state->maildir = &master_state->maildir;
    state->should_run = true;
    state->master_socket = sockets[0];
    state->worker_socket = sockets[1];
    state->current_sockets = 0;
    state->hostname = strdup(master_state->hostname);
    state->random_filenames = master_state->random_filenames;
    state->deliveries = 0;
    state->timeout_secs = master_state->timeout_secs;
    firedns_init(&state->dns_state);

    if (master_state->dns)
    {
        firedns_add_server(&state->dns_state, master_state->dns);
    }
    else
    {
        firedns_add_servers_from_resolv_conf(&state->dns_state);
    }

    state->connection_states = dynamic_vector_create(sizeof(smtp_connection_state_t), 32);

    int result = pthread_create(&state->thread_id, NULL,
                                &worker_thread_fn, (void*)state);
    if (result != 0)
    {
        close(state->master_socket);
        close(state->worker_socket);
        dynamic_vector_close(&state->connection_states);
        LOG("Error creating thread: %d\n", result);
        return false;
    }

    char name[16];
    snprintf(name, 16, "WORKER%d", id);
    pthread_setname_np(state->thread_id, name);

    return true;
}


bool spawn_log_process(smtp_master_thread_state_t *state, char *filename)
{
    int sockets[2];
    create_local_socket_pair(sockets);
    state->log_socket = sockets[0];

    pid_t logpid = fork();
    if (logpid == 0)
    {
        log_loop(sockets[1], filename);
    }
    else
    {
        set_log_socket(sockets[0]);
    }

    return true;
}

void stop_log_process(smtp_master_thread_state_t *state)
{
    LOG("Stopping log thread");
    send_stop_log();
    int status;
    waitpid(state->log_process, &status, 0); 
}

// Отправляем сокет работнику с наименьшим числом активных соединений
void send_socket_to_worker(smtp_master_thread_state_t *state, int client_socket)
{
    smtp_worker_thread_state_t *workers = state->threads;
    smtp_worker_thread_state_t *least_busy_worker = NULL;
    int least_sockets = INT_MAX;

    for (int i = 0; i < state->num_threads; i++)
    {
        smtp_worker_thread_state_t *worker = workers + i;
        int socket_count = atomic_load(&worker->current_sockets);

        if (socket_count < least_sockets)
        {
            least_sockets = socket_count;
            least_busy_worker = worker;
        }
    }
    assert(least_busy_worker);

    smtp_thread_command_t command =
    {
        .type = SMTP_THREAD_ACCEPT,
        .socket = client_socket
    };

    int sent_bytes = send(least_busy_worker->master_socket, &command, sizeof(command), 0);
    assert(sent_bytes == sizeof(command));
    atomic_fetch_add(&least_busy_worker->current_sockets, 1);
}


void stop_all_workers(smtp_master_thread_state_t *state)
{
    LOG("Stopping all workers");
    smtp_thread_command_t command =
    {
        .type = SMTP_THREAD_STOP,
        .socket = -1
    };

    for (int i = 0; i < state->num_threads; i++)
    {
        smtp_worker_thread_state_t *worker = state->threads + i;
        send(worker->master_socket, &command, sizeof(command), 0);
    }

    for (int i = 0; i < state->num_threads; i++)
    {
        smtp_worker_thread_state_t *worker = state->threads + i;
        pthread_join(worker->thread_id, NULL);
    }
}

static atomic_bool G_ShouldRun;

void sigint_handler(int signum)
{
    LOG("Caught signal %d", signum);
    atomic_store(&G_ShouldRun, false);
}

void server_loop(smtp_master_thread_state_t *state)
{
    atomic_store(&G_ShouldRun, true);

    struct sigaction sa = {};
    sa.sa_handler = &sigint_handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);


    // Блокируем SIGINT и SIGTERM, чтобы они не пришли не в то время
    sigset_t origset;
    sigset_t blockset;
    sigemptyset(&blockset);
    sigaddset(&blockset, SIGINT);
    sigaddset(&blockset, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &blockset, &origset);

    int ls = state->listen_socket;
    int ls6 = state->listen_socket_v6;
    fd_set read_fds;
    int maxfd = ls6 > ls ? ls6 : ls;
    while (G_ShouldRun)
    {
        FD_ZERO(&read_fds);
        FD_SET(ls, &read_fds);
        FD_SET(ls6, &read_fds);

        // Разблокируем сигналы на время pselect.
        // Если это были SIGINT или SIGTERM, то сработает соответствующий хэндлер.
        // Если они пришли во время выполнения pselect или до него, нас выбросит
        // с EINTR.
        int ready = pselect(maxfd + 1, &read_fds, NULL, NULL, NULL, &origset);
        if (ready < 0 && errno == EINTR)
        {
            LOG("Server select interrupted");
            continue;
        }

        if (ready < 0)
        {
            LOG("Server select error: %d", errno);
            return;
        }

        if (FD_ISSET(ls, &read_fds))
        {
            int client_socket = accept(ls, NULL, NULL);
            set_socket_nonblocking(client_socket, true);
            send_socket_to_worker(state, client_socket);
        }

        if (FD_ISSET(ls6, &read_fds))
        {
            int client_socket = accept(ls6, NULL, NULL);
            set_socket_nonblocking(client_socket, true);
            send_socket_to_worker(state, client_socket);
        }
    }
}

int smtp(int argc, char **argv)
{
    pthread_setname_np(pthread_self(), "SMTPMASTER");
    smtp_options_t options = {};
    smtp_master_thread_state_t state = {};

    if (!parse_options(argc, argv, &options))
    {
        printf("Usage: %s [-p port] [-m maildir] [-l log_file] [-t threads] [-d dns] [-r] [-s timeout_secs] \n", argv[0]);
        return -1;
    }

    spawn_log_process(&state, options.log);

    LOG("Starting SMTP server with %d threads on port %d, maildir %s, logfile %s and dns %s", (int)options.num_threads, (int)options.port,
        options.maildir, options.log, options.dns);

    free(options.log);
    int error;
    maildir_t maildir = maildir_open(options.maildir, &error);
    if (error != 0)
    {
        LOG("Maildir check failed: %d\n", error);
        return error;
    }
    free(options.maildir);

    state.maildir = maildir;
    state.num_threads = options.num_threads;
    state.threads = malloc(sizeof(smtp_worker_thread_state_t) * state.num_threads);
    state.dns = options.dns;
    state.hostname = options.hostname;
    state.random_filenames = options.random_filenames;
    state.timeout_secs = options.timeout_secs;

    if (!client_command_parser_init())
    {
        LOG("Could not make command parser!");
        return -1;
    }

    // Все сигналы обрабатывает только главный тред. Поэтому временно блокируем
    // все сигналы. После запуска дочерних тредов снимем блокировку.
    sigset_t mask;
    sigset_t oldmask;
    sigfillset(&mask);
    pthread_sigmask(SIG_SETMASK, &mask, &oldmask);
    for (int i = 0; i < state.num_threads; i++)
    {
        smtp_worker_thread_state_t *thread_state = state.threads + i;

        if (!spawn_worker_thread(i + 1, &state, thread_state))
        {
            LOG("Failed to spawn thread %d", i);
            return -1;
        }
    }
    pthread_sigmask(SIG_SETMASK, &oldmask, NULL);
    LOG("Spawned worker threads");

    if (!create_server_socket_ipv4("0.0.0.0", 7548, &state.listen_socket))
    {
        LOG("Failed to open server socket");
        return -1;
    }
    do_listen(state.listen_socket, 8);

    if (!create_server_socket_ipv6("::0", 7548, &state.listen_socket_v6))
    {
        LOG("Failed to open IPV6 server socket");
        return -1;
    }
    do_listen(state.listen_socket_v6, 8);

    LOG("Starting server loop");
    server_loop(&state);
    LOG("Finished server loop");

    stop_all_workers(&state);
    stop_log_process(&state);
    free(state.threads);
    free(state.dns);
    free(state.hostname);

    client_command_parser_free();

    return 0;
}