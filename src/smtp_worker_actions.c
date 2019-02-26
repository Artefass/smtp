#include <string.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <assert.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include "serverstate-fsm.h"
#include "smtp_worker.h"
#include "commands.h"
#include "server_replies.h"
#include "logger.h"
#include "util.h"
#include "maildir.h"

void create_file(smtp_worker_thread_state_t *thread_state, smtp_connection_state_t *state)
{
    if (thread_state->random_filenames)
    {
        int random_number;
        get_random(sizeof(int), (char*)&random_number);

        generate_filename(state->filename, sizeof(state->filename), thread_state->current_time, random_number, getpid(),
                thread_state->id, thread_state->deliveries, thread_state->hostname);
        state->fd = maildir_create_in_tmp(thread_state->maildir, state->filename);
        thread_state->deliveries++;
    }
    else
    {
        snprintf(state->filename, sizeof(state->filename), "%d.mail", (int)thread_state->deliveries);
        state->fd = maildir_create_in_tmp(thread_state->maildir, state->filename);
        thread_state->deliveries++;
    }
}

static bool same_address(firedns_state *state, const char *hostname, int fd)
{
    socklen_t socklen = sizeof(struct sockaddr_storage);
    struct sockaddr_storage addr;
    getpeername(fd, (struct sockaddr*)&addr, &socklen);
    if (addr.ss_family == AF_INET)
    {
        struct in_addr *resolved = firedns_resolveip4(state, hostname);
        struct sockaddr_in *s = (struct sockaddr_in*)&addr;
        return resolved->s_addr == s->sin_addr.s_addr;
    }
    else
    {
        struct in6_addr *resolved = firedns_resolveip6(state, hostname);
        struct sockaddr_in6 *s = (struct sockaddr_in6*)&addr;
        return memcmp(s->sin6_addr.s6_addr, resolved->s6_addr, sizeof(resolved->s6_addr)) == 0;
    }
}

static bool same_mx_address(firedns_state *state, const char *hostname, int fd)
{
    char *mx_name = firedns_resolvemx(state, hostname);
    if (!mx_name)
    {
        return false;
    }
    return same_address(state, mx_name, fd);
}

static void write_message(smtp_connection_state_t *state, const char *msg)
{
    int len = strlen(msg);
    dynamic_vector_copy_back(&state->write_buffer, msg, len);
    state->write_buffer.size = len;
}

static void write_message_format(smtp_connection_state_t *state, const char *format, ...)
{
    va_list args1;
    va_start(args1, format);
    int len = vsnprintf(NULL, 0, format, args1);
    va_end(args1);

    dynamic_vector_reserve_at_least(&state->write_buffer, len + 1);

    va_list args2;
    va_start(args2, format);
    vsnprintf(state->write_buffer.data, state->write_buffer.capacity, format, args2);
    va_end(args2);

    state->write_buffer.size = len;
}

void server_recv_helo(te_fsm_state new_state, smtp_worker_thread_state_t *thread_state,
        smtp_connection_state_t *state, command_match_t match)
{
    LOG("Processing HELO");

    char *domain = NULL;
    int len;
    if (!command_get_substring(&match, "domain", &domain, &len))
    {
        LOG("No domain provided in HELO");
        write_message(state, get_server_reply(REPLY_SYNTAX_ERROR_IN_PARAMS));
        state->mode = CONNECTION_WRITING;
        command_match_free(&match);
        return;
    }

#ifdef CHECK_LEGIT
    if (!same_mx_address(&thread_state->dns_state, domain, state->socket))
    {
        LOG("HELO domain's MX record and connected socket have different addresses");
        write_message(state, "421 Closing transmission channel\r\n");
        state->mode = CONNECTION_WRITING;
        state->state = FSM_ST_QUITTED;
        command_free_substring(domain);
        command_match_free(&match);
        return;
    }
#endif // CHECK_LEGIT
    write_message_format(state, "250 %s greets %s\r\n", thread_state->hostname, domain);
    state->mode = CONNECTION_WRITING;
    state->state = new_state;

    command_free_substring(domain);
    command_match_free(&match);
}

void server_recv_ehlo(te_fsm_state new_state, smtp_worker_thread_state_t *thread_state,
        smtp_connection_state_t *state, command_match_t match)
{
    LOG("Processing EHLO");

    char *domain = NULL;
    int len;
    if (!command_get_substring(&match, "domain", &domain, &len))
    {
        LOG("No domain provided in EHLO");
        write_message(state, get_server_reply(REPLY_SYNTAX_ERROR_IN_PARAMS));
        state->mode = CONNECTION_WRITING;
        command_match_free(&match);
        return;
    }

#ifdef CHECK_LEGIT
    if (!same_mx_address(&thread_state->dns_state, domain, state->socket))
    {
        LOG("EHLO domain's MX record and connected socket have different addresses, aborting.");
        write_message(state, "421 Closing transmission channel\r\n");
        state->mode = CONNECTION_WRITING;
        state->state = FSM_ST_QUITTED;
        command_free_substring(domain);
        command_match_free(&match);
        return;
    }
#endif // CHECK_LEGIT

    write_message_format(state, "250-%s greets %s\r\n250 VRFY\r\n", thread_state->hostname, domain);
    state->mode = CONNECTION_WRITING;
    state->state = new_state;
    command_free_substring(domain);
    command_match_free(&match);
}


void server_recv_vrfy(te_fsm_state new_state, smtp_worker_thread_state_t *thread_state,
        smtp_connection_state_t *state, command_match_t match)
{
    (void)thread_state;
    LOG("Processing VRFY");
    write_message(state, "550 No such user here \r\n");
    state->mode = CONNECTION_WRITING;
    state->state = new_state;
    command_match_free(&match);
}

void server_recv_mailfrom(te_fsm_state new_state, smtp_worker_thread_state_t *thread_state,
        smtp_connection_state_t *state, command_match_t match)
{
    LOG("Processing MAIL FROM");
    (void)thread_state;
    write_message(state, "250 OK\r\n");
    state->mode = CONNECTION_WRITING;
    state->from = match;
    state->state = new_state;
}

void server_recv_rcptto(te_fsm_state new_state, smtp_worker_thread_state_t *thread_state,
        smtp_connection_state_t *state, command_match_t match)
{
    LOG("Processing RCPT TO");
    (void)thread_state;
    command_match_free(&match);
    write_message(state, "250 OK\r\n");
    state->mode = CONNECTION_WRITING;
    dynamic_vector_copy_back(&state->recepient_buffer, state->current_message->text, 
            state->current_message->len);

    message_t msg_copy;
    msg_copy.text = state->recepient_buffer.data;
    msg_copy.len = state->recepient_buffer.size;
    state->recepient = client_command_parse_message(&msg_copy);
    assert(state->recepient.command == CLIENT_RCPTTO);

    state->state = new_state;
}

void server_recv_rset(te_fsm_state new_state, smtp_worker_thread_state_t *thread_state,
        smtp_connection_state_t *state, command_match_t match)
{
    (void)thread_state;
    LOG("Processing RSET");

    if (state->recepient.match_data)
    {
        command_match_free(&state->recepient);
    }

    if (state->from.match_data)
    {
        command_match_free(&state->from);
    }

    dynamic_vector_clear(&state->recepient_buffer);

    write_message(state, "250 OK\r\n");
    state->mode = CONNECTION_WRITING;
    state->state = new_state;
    command_match_free(&match);
}

void server_recv_unknown(te_fsm_state new_state, smtp_worker_thread_state_t *thread_state,
        smtp_connection_state_t *state, command_match_t match)
{
    LOG("Processing unknown command");
    (void)thread_state;
    write_message(state, "500 Syntax error, command unrecognized\r\n");
    state->mode = CONNECTION_WRITING;
    state->state = new_state;
    command_match_free(&match);
}

void server_recv_error(te_fsm_state new_state, smtp_worker_thread_state_t *thread_state,
        smtp_connection_state_t *state, command_match_t match)
{
    LOG("Processing bad command");
    (void)thread_state;
    write_message(state, "503 Bad sequence of commands\r\n");
    state->mode = CONNECTION_WRITING;
    state->state = new_state;
    command_match_free(&match);
}

void move_file_to_destination(smtp_worker_thread_state_t *thread_state,
        smtp_connection_state_t *state)
{
    char *dst_domain;
    int dst_domain_len;
    command_get_substring(&state->recepient, "domain", &dst_domain, &dst_domain_len);
    if (!dst_domain || dst_domain_len <= 0)
    {
        LOG("No domain. Leaving mail in tmp");
        return;
    }

    if (strlen(thread_state->hostname) == (unsigned long)dst_domain_len 
            && memcmp(thread_state->hostname, dst_domain, dst_domain_len) == 0)
    {
        maildir_move_to_cur(thread_state->maildir, state->filename);
    }
    else
    {
        maildir_move_to_relay(thread_state->maildir, state->filename);
    }

    command_free_substring(dst_domain);
}

void server_recv_enddata(te_fsm_state new_state, smtp_worker_thread_state_t *thread_state,
        smtp_connection_state_t *state, command_match_t match)
{
    LOG("Processing end of DATA");
    assert(state->is_receiving_data == true);

    write_message(state, "250 OK\r\n");
    state->is_receiving_data = false;
    state->mode = CONNECTION_WRITING;
    state->state = new_state;
    command_match_free(&match);

    move_file_to_destination(thread_state, state);
    close(state->fd);
    state->fd = 0;

    command_match_free(&state->from);
    command_match_free(&state->recepient);
    dynamic_vector_clear(&state->recepient_buffer);
}

void server_recv_data(te_fsm_state new_state, smtp_worker_thread_state_t *thread_state,
        smtp_connection_state_t *state, command_match_t match)
{
    assert(state->is_receiving_data == false);
    LOG("Processing DATA");
    (void)thread_state;
    state->mode = CONNECTION_WRITING;
    state->state = new_state;
    state->is_receiving_data = true;
    write_message(state, "354 Start mail input; end with <CRLF>.<CRLF>\r\n");
    command_match_free(&match);

    create_file(thread_state, state);
}


void server_recv_quit(te_fsm_state new_state, smtp_worker_thread_state_t *thread_state,
        smtp_connection_state_t *state, command_match_t match)
{
    LOG("Processing QUIT");
    (void)thread_state;
    write_message_format(state, "221 %s Service closing transmission channel\r\n", thread_state->hostname);
    state->mode = CONNECTION_WRITING;
    state->state = new_state;
    command_match_free(&match);
}

void server_timeout(te_fsm_state new_state, smtp_connection_state_t *state)
{
    LOG("Processing timeout");
    write_message(state, "421 Closing transmission channel\r\n");
    state->mode = CONNECTION_WRITING;
    state->state = new_state;
}


