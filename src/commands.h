#pragma once

#define PCRE2_CODE_UNIT_WIDTH 8

#include <stdio.h>
#include <string.h>
#include <pcre2.h>
#include <stdbool.h>
#include "message_builder.h"


/**
 * @file
 * @brief Парсинг сообщений, пришедших с клиента.
 */

/**
 * Команды, которые нам может послать клиент
 */
typedef enum smtp_client_command
{
    CLIENT_UNKNOWN_COMMAND = 0,
    CLIENT_HELO,
    CLIENT_EHLO,
    CLIENT_VRFY,
    CLIENT_RSET,
    CLIENT_QUIT,
    CLIENT_MAILFROM,
    CLIENT_RCPTTO,
    CLIENT_DATA,
    CLIENT_ENDDATA,
    CLIENT_COMMAND_COUNT
} smtp_client_command;

/**
 * Описание одной команды
 */
typedef struct command_regex_t
{
    /**
     * Тип команды
     */
    enum smtp_client_command command;
    /**
     * Имя команды
     */
    const char *name;
    /**
     * Регулярное выражение команды
     */
    PCRE2_SPTR pattern;
    /**
     * Скомпилированный код выражения
     */
    pcre2_code *re;
} command_regex_t;


/**
 * "Совпадение" строчки по регулярному выражению.
 * Позволяет вытащить нужную информацию в подстроку.
 */
typedef struct command_match_t
{
    /**
     * Тип совпавшей команды
     */
    enum smtp_client_command command;
    /**
     * Данные о совпадении
     */
    pcre2_match_data *match_data;
} command_match_t;


/**
 * Инициализация структуры парсинга команд клиента
 */
bool client_command_parser_init();
/**
 * Деинициализации стуктуры парсинга команд клиента
 */
void client_command_parser_free();
/**
 * Попытка распарсить сообщение \p message с помощью описания
 * команды \p regex с записью результата в \p match.
 * В случае успеха \p match нужно освободить с помощью
 * #command_match_free
 * \returns true, если команда совпала, false иначе.
 */
bool try_parse_message(message_t *message, command_regex_t *regex, 
        command_match_t *match);
/**
 * Парсинг сообщения \p message, полученного от клиента.
 * \returns совпадение с установленным типом команды.
 * В случае неудачи тип будет #CLIENT_UNKNOWN_COMMAND 
 * Возвращённое значение следует освободить с помощью #command_match_free.
 */
command_match_t client_command_parse_message(message_t *message);
/**
 * Освобождение данных совпадения.
 */
void command_match_free(command_match_t *match);
/**
 * Проверка, есть ли в совпадении \p match группа с именем \p name
 */
bool command_has_substring(command_match_t *match, const char *name);
/**
 * Получение подстроки в совпадении \p match с именем \name
 * \param buffer Возвращённый буфер
 * \param buflen Длина возвращённого буфера
 * \returns true, если в \p match есть группа \p name, false иначе
 * В случае успеха возвращённу строку следует освободить с 
 * помощью #command_free_substring
 */
bool command_get_substring(command_match_t *match, const char *name, char **buffer, int *buflen);
/**
 * Освобождает подстроку, полученную в #command_get_substring
 */
void command_free_substring(const char *substring);
/**
 * Записывает команды в файл
 */
void print_commands(const char *filename);