#pragma once

#include <regex.h>

/*
 * список команд клиента, которые может передать клиент
 */ 
typedef enum client_command_t {
    UNKNOWN= 0,    
    HELO,
    EHLO,
    MAIL,
    RCPT,
    DATA,
    RSET,
    QUIT
} client_command_t;

/*
 * "найденная команда"
 */
typedef struct command_match_t {
    /*
     * тип команды
     */
    client_command_t command;
    /*
     * даные о найденном совпадении
     */
    char *match_data;
} command_match_t;

/*
 * описание одной команды
 */
typedef struct command_regex_t {
    /*
     * тип команды
     */
    client_command_t command;

    /*
     * имя команды
     */
    char *name;

    /*
     * шаблон регулярного выражения 
     */
    char pattern[];

    /**
     * скомпилированное регулярное выражение
     */
    regex_t *re;
} command_regex_t;

void match_command(char *message, int *error);

void find_command(command_match_t match_command);

