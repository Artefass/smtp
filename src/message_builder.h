#pragma once
#include <stdbool.h>
#include "dynamic_vector.h"

/**
 * @file
 * @brief Составление целой команды SMTP из разрозненных
 * кусков, пришедших по сети.
 */


/**
 * Создаёт полные сообщения из полученных строк.
 * Например, в одном пакете придёт "ABC", а во втором "DEF\r\nGHIJ\r\n".
 * С помощью #message_builder_add_string и #message_builder_get_message можно
 * получить два сообщения: "ABCDEF\r\n" и "GHIJ\r\n".
 */
typedef struct message_builder_t
{
    /**
     * Буфер символов, в котором хранится необработанное сообщение
     */
    dynamic_vector_t unparsed_message_buffer;
    /**
     * Вектор #message_t, в котором хранятся готовые сообщения
     */
    dynamic_vector_t parsed_message_buffer;
} message_builder_t;


/**
 * Полное сообщение (строка, оканчивающаяся "\r\n")
 */
typedef struct message_t
{
    /**
     * Длина сообщения
     */
    int len;
    /**
     * Текст сообщения.
     */
    char *text;
} message_t;


/**
 * Создаёт #message_builder_t
 */
message_builder_t create_message_builder();

/**
 * Добавляет чанк в #message_builder_t.
 */
void message_builder_add_string(message_builder_t *builder, const char *str, int len);

/**
 * Закрывает #message_builder_t и освобождает всю связанную с ним память.
 */
void message_builder_close(message_builder_t *builder);

/**
 * Возвращает количество готовых сообщений
 */
int message_builder_ready_message_count(message_builder_t *builder);

/**
 * Извлекает сообщение из #message_builder_t.
 * 
 * Возвращённое сообщение следует освободить с помощью #message_free.
 */
message_t message_builder_get_message(message_builder_t *builder);

/**
 * Освобождает сообщение, полученное из #message_builder_get_message.
 */
void message_free(message_t *message);