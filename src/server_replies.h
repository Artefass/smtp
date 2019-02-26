#pragma once

/**
 * @file
 * @brief Сообщения и коды ответов SMTP сервера
 */

/**
 * Коды ответов сервера
 */
enum smtp_reply_code
{
    REPLY_READY = 220,
    REPLY_CLOSING_TRANSM = 221,
    REPLY_OK = 250,
    REPLY_USER_NOT_LOCAL = 251,
    REPLY_START_INPUT = 354,
    REPLY_ACTION_ABORTED = 451,
    REPLY_UNKNOWN_COMMAND = 500,
    REPLY_SYNTAX_ERROR_IN_PARAMS = 501,
    REPLY_COMMAND_NOT_IMPLEMENTED = 502,
    REPLY_BAD_SEQUENCE_OF_COMMANDS = 503,    
};

typedef struct smtp_reply
{
    int code;
    const char* text;
} smtp_reply;

const char *get_server_reply(int code);
