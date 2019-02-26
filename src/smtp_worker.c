#include "smtp_worker.h"
#include "smtp_sockets.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>
#include <assert.h>
#include "logger.h"
#include "commands.h"
#include "util.h"
#include "server_replies.h"


typedef struct command_event_entry_t
{
    enum smtp_client_command command;
    te_fsm_event event;

} command_event_entry_t;

command_event_entry_t command_to_event_table[] =
{
    { CLIENT_UNKNOWN_COMMAND, FSM_EV_UNKNOWN },
    { CLIENT_HELO, FSM_EV_HELO },
    { CLIENT_EHLO, FSM_EV_EHLO },
    { CLIENT_QUIT, FSM_EV_QUIT },
    { CLIENT_RSET, FSM_EV_RSET },
    { CLIENT_VRFY, FSM_EV_VRFY },
    { CLIENT_RCPTTO, FSM_EV_RCPT_TO },
    { CLIENT_MAILFROM, FSM_EV_MAIL_FROM },
    { CLIENT_DATA, FSM_EV_DATA },
    { CLIENT_ENDDATA, FSM_EV_ENDDATA }
};


time_t calculate_sleep_time(smtp_connection_state_t *connection_states, int num, time_t current_time)
{
    // Спим максимум 30 секунд. Мало ли что
    time_t to_sleep = 30;
    for (int i = 0; i < num; i++)
    {
        smtp_connection_state_t *state = connection_states + i;
        if (state->mode != CONNECTION_READING) continue; // Нет таймаута на посылку данных

        if (state->timeout < current_time)
        {
            to_sleep = 0;
            break;
        }

        if (state->timeout - current_time < to_sleep)
        {
            to_sleep = state->timeout - current_time;
        }
    }

    return to_sleep;
}

void handle_master_command(smtp_worker_thread_state_t *thread_state)
{
    smtp_thread_command_t command = {0};
    size_t read_size = read(thread_state->worker_socket, &command, sizeof(smtp_thread_command_t));
    if (read_size == 0 || command.type == SMTP_THREAD_STOP) // соединение с мастер-тредом оборвалось
    {
        LOG("Master sent stop command");
        thread_state->should_run = false;
    }
    else if (command.type == SMTP_THREAD_ACCEPT)
    {
        smtp_connection_state_t new_connection = {0};
        new_connection.is_receiving_data = false;
        new_connection.state = FSM_ST_INIT;
        new_connection.timeout = thread_state->current_time.tv_sec + (time_t)thread_state->timeout_secs;
        new_connection.socket = command.socket;
        new_connection.write_buffer = dynamic_vector_create(sizeof(char), 256);
        new_connection.write_buffer_pos = 0;
        new_connection.read_buffer = dynamic_vector_create(sizeof(char), 256);
        new_connection.recepient_buffer = dynamic_vector_create(sizeof(char), 256);
        new_connection.mb = create_message_builder();

        const char *greeting = get_server_reply(REPLY_READY);
        int len = snprintf(new_connection.write_buffer.data, new_connection.write_buffer.capacity, greeting, thread_state->hostname);
        if (len >= new_connection.write_buffer.capacity)
        {
            dynamic_vector_reserve_at_least(&new_connection.write_buffer, len + 1);
            snprintf(new_connection.write_buffer.data, new_connection.write_buffer.capacity, greeting, thread_state->hostname);
        }

        new_connection.write_buffer.size = len;
        new_connection.mode = CONNECTION_WRITING;
        dynamic_vector_copy_elem_back(&thread_state->connection_states, &new_connection);
    }
    else
    {
        assert(0);
    }
}

void close_connection_state(smtp_connection_state_t *state)
{
    close(state->fd);
    close(state->socket);
    
    if (state->recepient.match_data)
    {
        command_match_free(&state->recepient);
    }

    if (state->from.match_data)
    {
        command_match_free(&state->from);
    }

    dynamic_vector_close(&state->write_buffer);
    dynamic_vector_close(&state->read_buffer);
    dynamic_vector_close(&state->recepient_buffer);
    message_builder_close(&state->mb);

    if (state->fd > 0)
    {
        close(state->fd);
    }
}

bool write_to_client(smtp_connection_state_t *connection_state)
{
    assert(connection_state->mode == CONNECTION_WRITING);
    char *msg = connection_state->write_buffer.data + connection_state->write_buffer_pos;
    int len = connection_state->write_buffer.size - connection_state->write_buffer_pos;
    assert(len > 0);

    int sent_bytes = send_to_socket(connection_state->socket, msg, len);
    if (sent_bytes < 0)
    {
        LOG("Error writing data");
        return false;
    }
    connection_state->write_buffer_pos += sent_bytes;

    if (connection_state->write_buffer_pos == connection_state->write_buffer.size)
    {
        connection_state->mode = CONNECTION_READING;
        connection_state->write_buffer_pos = 0;
        dynamic_vector_clear(&connection_state->write_buffer);
    }

    return true;
}


bool read_from_client(smtp_connection_state_t *connection_state)
{
    assert(connection_state->mode == CONNECTION_READING);

    int len = read_from_socket(connection_state->socket, connection_state->read_buffer.data, connection_state->read_buffer.capacity);
    if (len <= 0)
    {
        LOG("Error reading data");
        return false;
    }
    //LOG("Received message %.*s", len, connection_state->read_buffer.data);

    message_builder_add_string(&connection_state->mb, connection_state->read_buffer.data, len);

    return true;
}



void handle_message_data(smtp_worker_thread_state_t *worker_state, smtp_connection_state_t *conn_state, message_t *msg)
{
    assert(conn_state->is_receiving_data);
    // Приняли точку - конец сообщения
    if (msg->len == 1 && msg->text[0] == '.')
    {
        fsm_step(conn_state->state, FSM_EV_ENDDATA, conn_state, worker_state, NULL);
    }
    else
    {
        write(conn_state->fd, msg->text, msg->len);
        write(conn_state->fd, "\r\n", 2);
    }
}

te_fsm_event find_event(smtp_client_command command)
{
    int n = ARRAYNUM(command_to_event_table);

    for (int i = 0; i < n; i++)
    {
        if (command_to_event_table[i].command == command)
        {
            return command_to_event_table[i].event;
        }
    }

    return FSM_EV_UNKNOWN;
}

void handle_message(smtp_worker_thread_state_t *worker_state, smtp_connection_state_t *conn_state, message_t *msg)
{
    assert(!conn_state->is_receiving_data);
    command_match_t match = client_command_parse_message(msg);
    te_fsm_event event = find_event(match.command);
    fsm_step(conn_state->state, event, conn_state, worker_state, &match);
}

void handle_timeout(smtp_worker_thread_state_t *worker_state, smtp_connection_state_t *conn_state)
{
    assert(worker_state->current_time.tv_sec > conn_state->timeout);
    fsm_step(conn_state->state, FSM_EV_TIMEOUT, conn_state, worker_state, NULL); 
}

void worker_loop(smtp_worker_thread_state_t *state)
{
    smtp_connection_state_t *connection_states = (smtp_connection_state_t*)state->connection_states.data;
    fd_set readfds;
    fd_set writefds;
    while (state->should_run)
    {
        int maxfd = state->worker_socket;
        state->current_time = smtpgettime();
        time_t current_time_sec = state->current_time.tv_sec;
        FD_ZERO(&readfds);
        FD_ZERO(&writefds);

        FD_SET(state->worker_socket, &readfds);

        for (int i = 0; i < state->connection_states.size; i++)
        {
            smtp_connection_state_t *connection_state = connection_states + i;
            if(connection_state->mode == CONNECTION_WRITING)
            {
                FD_SET(connection_state->socket, &writefds);
            }
            else
            {
                FD_SET(connection_state->socket, &readfds);
            }

            if (connection_state->socket > maxfd)
            {
                maxfd = connection_state->socket;
            }
        }

        struct timespec sleep_time;
        sleep_time.tv_sec = calculate_sleep_time(connection_states,
                state->connection_states.size, current_time_sec);
        sleep_time.tv_nsec = 0;
        

        int ready = pselect(maxfd + 1, &readfds, &writefds, NULL, &sleep_time, NULL);

        if (ready < 0)
        {
            printf("PSELECT ERROR: %d\n", errno);
            return;
        }

        for (int i = 0; i < state->connection_states.size; i++)
        {
            smtp_connection_state_t *connection_state = connection_states + i;
            if (connection_state->mode == CONNECTION_WRITING && FD_ISSET(connection_state->socket, &writefds))
            {
                connection_state->timeout = current_time_sec + state->timeout_secs;
                write_to_client(connection_state);
            }
            else if (connection_state->mode == CONNECTION_READING && FD_ISSET(connection_state->socket, &readfds))
            {
                connection_state->timeout = current_time_sec + state->timeout_secs;
                if (!read_from_client(connection_state))
                {
                    LOG("Client disconnected by himself");
                    connection_state->state = FSM_ST_QUITTED;
                    continue;
                }
                int messages = message_builder_ready_message_count(&connection_state->mb);
                for (int i = 0; i < messages; i++)
                {
                    message_t msg = message_builder_get_message(&connection_state->mb);
                    connection_state->current_message = &msg;
                    if (connection_state->is_receiving_data)
                    {
                        handle_message_data(state, connection_state, &msg);
                    }
                    else
                    {
                        handle_message(state, connection_state, &msg);
                    }
                    connection_state->current_message = NULL;
                    message_free(&msg);
                }
            }
            else if (connection_state->mode == CONNECTION_READING && current_time_sec > connection_state->timeout)
            {
                LOG("Connection timed out %d %d", (int)connection_state->timeout, (int)current_time_sec);
                handle_timeout(state, connection_state);
            }
            LOG("Connection %d new state %d", i, (int)connection_state->state);
        }

        int size = state->connection_states.size;
        for (int i = 0; i < size; i++)
        {
            smtp_connection_state_t *connection_state = connection_states + i;
            if (connection_state->mode == CONNECTION_READING && connection_state->state == FSM_ST_QUITTED)
            {
                LOG("Closing a connection");
                close_connection_state(connection_state);
                dynamic_vector_delete_at(&state->connection_states, i);
                size--;
            }
        }
#if 0
        for (int i = 0; i < size; i++)
        {
            smtp_connection_state_t *connection_state = connection_states + i;
            if (connection_state->mode == CONNECTION_READING && connection_state->timeout > current_time_sec)
            {
                
            }
        }
#endif

        if (FD_ISSET(state->worker_socket, &readfds))
        {
            handle_master_command(state);
            if (!state->should_run) break;
        }
    }
    LOG("Closing everything");

    for (int i = 0; i < state->connection_states.size; i++)
    {
        smtp_connection_state_t *connection_state = connection_states + i;
        close_connection_state(connection_state);
    }


    dynamic_vector_close(&state->connection_states);
    maildir_close(state->maildir);
    close(state->master_socket);
    close(state->worker_socket);
    free(state->hostname);

    LOG("Shutting down");
}

