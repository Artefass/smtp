#include "commands.h"
#include "util.h"
#include <assert.h>

#define DEFINE_COMMAND(cmd, name, str) (command_regex_t){(cmd), (name), (PCRE2_SPTR)(str), NULL}

#define SUBDOMAIN "\\w(?:[\\w\\-]*\\w)"
#define DOMAIN SUBDOMAIN "(?:\\." SUBDOMAIN ")*"

#define IPV4_LITERAL "\\d{1,3}(?:\\.\\d{1,3}){3}"

// TODO: Пока поддерживаем толькол полные ipv6 адреса

#define V6HEX "[0-9a-fA-F]{1,4}"
#define IPV6_FULL V6HEX "(?::" V6HEX "){7}"
#define IPV6_LITERAL "IPv6:(?<ipv6>" IPV6_FULL ")"

#define DOMAIN_OR_LITERAL "(?:(?<domain>" DOMAIN ")|(?:\\[(?<ipv4>" IPV4_LITERAL ")\\])|(?:\\[(?<ipv6_literal>" IPV6_LITERAL ")\\]))"

// TODO: Не распознаём at-domain нотацию в письмах (@a,@b в <@a,@b:user@d>).
// В SMTP (пункт 4.1.2) она SHOULD be ignored, хотя MUST be accepted.

#define ATEXT "[a-zA-Z0-9!#$%&'*\\+\\-\\/=?^_`{|}~]"
#define DOTSTRING "(?:" ATEXT ")+(?:\\." ATEXT "+)*"

// TODO: Распознаём только Dot-string нотацию для локальной части (user.test@mail.com)
#define LOCALPART DOTSTRING

#define MAILBOX "(?<localpart>" LOCALPART ")@" DOMAIN_OR_LITERAL
#define PATH "<" MAILBOX ">"
#define REVERSEPATH "(?:(?<emptyrpath><>)|(?<rpath>" PATH "))"

#define POSTMASTER "(?i)Postmaster(?-i)"

#define RECEPIENTS "(?:(?<thispostmaster><" POSTMASTER ">)|(?<otherpostmaster><" POSTMASTER "@" DOMAIN ">)|(?<fpath>" PATH "))"


static bool init_done = false;

static command_regex_t client_commands[] =
{
    // "HELO" SP domain
    DEFINE_COMMAND(CLIENT_HELO, "HELO", "^(HELO|helo)\\s(?<domain>" DOMAIN ")$"),
    // "EHLO" SP (Domain / address-literal)
    DEFINE_COMMAND(CLIENT_EHLO, "EHLO", "^(EHLO|ehlo)\\s" DOMAIN_OR_LITERAL "$"),
    // "VRFY" SP String
    DEFINE_COMMAND(CLIENT_VRFY, "VRFY", "^(VRFY|vrfy)\\s.*$"),
    // "RSET"
    DEFINE_COMMAND(CLIENT_RSET, "RSET", "^(RSET|rset)$"),
    // "QUIT"
    DEFINE_COMMAND(CLIENT_QUIT, "QUIT", "^(QUIT|quit)$"),
    // TODO: не распознаём Mail-Parameters
    // "MAIL FROM:" Reverse-path [SP Mail-Parameters]
    DEFINE_COMMAND(CLIENT_MAILFROM, "MAIL FROM", "^(MAIL|mail) (FROM|from):(?<sender>" REVERSEPATH ")$"),
    // TODO: не рапспознаём Rcpt-parameters
    // "RCPT TO:" ("<Postmaster@" Domain ">" / "<Postmaster>"
    // / Forward-path ) [ SP Rcpt-parameters]
    DEFINE_COMMAND(CLIENT_RCPTTO, "RCPT TO", "^(RCPT|rcpt) (TO|to):(?<recepient>" RECEPIENTS ")$"),
    // Начала приёма тела письма
    DEFINE_COMMAND(CLIENT_DATA, "DATA", "^(DATA|data)$"),
    // Конец приёма тела письма - просто строка с точкой.
    DEFINE_COMMAND(CLIENT_ENDDATA, "ENDDATA", "^\\.$")
};

bool client_command_parser_init()
{
    if (init_done) return true;    

    int n = ARRAYNUM(client_commands);
    for (int i = 0; i < n; i++)
    {
        command_regex_t *cmd = client_commands + i;
        size_t error_offset = 0;
        int error_number = 0;
        
        cmd->re = pcre2_compile(cmd->pattern, PCRE2_ZERO_TERMINATED,
                0, &error_number, &error_offset, NULL);

        if (cmd->re == NULL)
        {
            PCRE2_UCHAR buffer[256];
            pcre2_get_error_message(error_number, buffer, sizeof(buffer));
            printf("PCRE2 compilation of string %s failed at offset %d: %s\n",
                    cmd->pattern, (int)error_offset, buffer);
        }
    }
    
    init_done = true;
    return true;
}

void print_commands(const char *filename)
{
    FILE *commandsFile = fopen(filename, "w");
    int n = ARRAYNUM(client_commands);
    for (int i = 0; i < n; i++)
    {
        command_regex_t *cmd = client_commands + i;
        
        fprintf(commandsFile, "%s\n%s\n", cmd->name, cmd->pattern);
    }
    fclose(commandsFile);
}

void client_command_parser_free()
{
    int n = ARRAYNUM(client_commands);
    for (int i = 0; i < n; i++)
    {
        command_regex_t *cmd = client_commands + i;
        pcre2_code_free(cmd->re);
    }
}


bool try_parse_message(message_t *message, command_regex_t *regex, command_match_t *match)
{
    assert(message && regex && match);
    assert(!match->match_data);
    pcre2_match_data *match_data = pcre2_match_data_create_from_pattern(regex->re, NULL); 
    
    int result = pcre2_match(regex->re, (PCRE2_SPTR)message->text, message->len,
            0, 0, match_data, NULL);

    if (result < 0)
    {
        if (result != PCRE2_ERROR_NOMATCH)
        {
            // TODO: запись в лог
            char buf[1024];
            pcre2_get_error_message(result, (PCRE2_UCHAR*)buf, 1024);
            printf("PCRE2 ERROR: %s\n", buf);
        }
        pcre2_match_data_free(match_data);
        match->match_data = NULL;
        match->command = CLIENT_UNKNOWN_COMMAND;
        return false;
    }
    
    match->match_data = match_data;
    match->command = regex->command;

    return true; 
} 

command_match_t client_command_parse_message(message_t *message)
{
    command_match_t match = {0}; 

    int n = ARRAYNUM(client_commands);
    for (int i = 0; i < n; i++)
    {
        command_regex_t *command = client_commands + i;
        bool success = try_parse_message(message, command, &match);

        if (success)
        {
            return match;
        }
    }

    match.command = CLIENT_UNKNOWN_COMMAND;
    match.match_data = NULL;
    return match;
}

bool command_has_substring(command_match_t *match, const char *name)
{
    PCRE2_SIZE size;
    int result = pcre2_substring_length_byname(match->match_data, (PCRE2_SPTR)name, &size);
    return result >= 0 && size > 0;
}

bool command_get_substring(command_match_t *match, const char *name, char **buffer, int *buflen)
{
    PCRE2_SIZE size;
    int result = pcre2_substring_get_byname(match->match_data, (PCRE2_SPTR)name, (PCRE2_UCHAR**)buffer, &size);
    if (result < 0)
    {
        return false;
    }
    *buflen = size;
    return true;
}

void command_free_substring(const char *substring)
{
    pcre2_substring_free((PCRE2_UCHAR*)substring);
}


void command_match_free(command_match_t *match)
{
    match->command = CLIENT_UNKNOWN_COMMAND;
    pcre2_match_data_free(match->match_data);
    match->match_data = NULL;
}