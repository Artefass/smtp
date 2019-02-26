#pragma once
#include <stdbool.h>

/**
 * @file
 * @brief Операции с рабочим потоком
 */

#include "serverstate-fsm.h"
#include "smtp_time.h"
#include "dynamic_vector.h"
#include "maildir.h"
#include "message_builder.h"
#include "commands.h"

#include <stdbool.h>
#include <stdatomic.h>
#include <pthread.h>
#include <firedns.h>
#include <time.h>


/**
 * Текущее состояние сокета соединения: пишем или получаем
 */
enum smtp_connection_mode
{
    CONNECTION_READING,
    CONNECTION_WRITING,
};


/**
 * Состояние соединения
 */
typedef struct smtp_connection_state_t
{
    /**
     * Текущее состояние в конечном автомате
     */
    te_fsm_state state;
    /**
     * Сокет взаимодействия с клиентом
     */
    int socket;
    /**
     * Состояние @a socket
     */
    enum smtp_connection_mode mode;
    /**
     * Получаем команды или данные письма
     */
    bool is_receiving_data;
    
    /**
     * Время наступления тайм-аута
     */
    time_t timeout;

    
    /**
     * Буфер данных, которые должны отправить
     */
    dynamic_vector_t write_buffer;
    /**
     * Сколько байт уже отправили
     */
    int write_buffer_pos;

    /**
     * Буфер прочитанных данных
     */
    dynamic_vector_t read_buffer;

    /**
     * Дескриптор файла письма
     */
    int fd;
    
    /**
     * Собиратель сообщений клиента из прочитанных данных
     */
    message_builder_t mb;
    
    /**
     * Соответствие строки отправителя
     */
    command_match_t from;
    // TODO: более одного получателя
    /**
     * Буфер, в котором храним строку получателя
     */
    dynamic_vector_t recepient_buffer;
    /**
     * Структура регэксп-матча получателя (нужна для вытаскивания параметров)
     */
    command_match_t recepient;
    
    /**
     * Имя файла
     */
    char filename[128];
    
    /**
     * Текущее полученное сообщение
     */
    message_t *current_message;
} smtp_connection_state_t;


/**
 * Состояние рабочего потока
 */
typedef struct smtp_worker_thread_state_t
{
    /**
     * Идентификатор
     */
    int id;
    pthread_t thread_id;
    /**
     * Число подключений
     */
    atomic_int current_sockets;
    
    /**
     * Сокет, в который главный тред записывает команды
     */
    int master_socket;
    /**
     * Сокет, из которого рабочий поток читает команды главного потока
     */
    int worker_socket;

    /**
     * Вектор для хранения состояния соединений
     */
    dynamic_vector_t connection_states;
    /**
     * Прекращение работы
     */
    volatile bool should_run;

    /**
     * Структура папки писем
     */
    maildir_t *maildir;
    
    /**
     * Включение генерации случайных имён файлов писем
     */
    bool random_filenames;
    /**
     * Количество полученных писем
     */
    int deliveries;
    /**
     * Имя текущего сервера
     */
    char *hostname;
    /**
     * Текущее время сервера
     */
    struct timespec current_time;
    /**
     * Структура для DNS
     */
    firedns_state dns_state;
    /**
     * Настройка таймаута в секундах
     */
    int timeout_secs;
} smtp_worker_thread_state_t;

/**
 * Тип команды главного треду рабочему
 */
enum smtp_thread_command_type
{
    SMTP_THREAD_STOP,
    SMTP_THREAD_ACCEPT
};

/**
 * Команда главного треду рабочему
 */
typedef struct smtp_thread_command_t
{
    /**
     * Тип команды
     */
    enum smtp_thread_command_type type;
    /**
     * Дескриптор с новым соединением, если команда SMTP_THREAD_ACCEPT
     */
    int socket;
} smtp_thread_command_t;


void worker_loop(smtp_worker_thread_state_t *state);

void server_recv_helo(te_fsm_state new_state, smtp_worker_thread_state_t *thread_state,
        smtp_connection_state_t *state, command_match_t match);
void server_recv_ehlo(te_fsm_state new_state, smtp_worker_thread_state_t *thread_state,
        smtp_connection_state_t *state, command_match_t match);
void server_recv_vrfy(te_fsm_state new_state, smtp_worker_thread_state_t *thread_state,
        smtp_connection_state_t *state, command_match_t match);
void server_recv_mailfrom(te_fsm_state new_state, smtp_worker_thread_state_t *thread_state,
        smtp_connection_state_t *state, command_match_t match);
void server_recv_rcptto(te_fsm_state new_state, smtp_worker_thread_state_t *thread_state,
        smtp_connection_state_t *state, command_match_t match);
void server_recv_rset(te_fsm_state new_state, smtp_worker_thread_state_t *thread_state,
        smtp_connection_state_t *state, command_match_t match);
void server_recv_unknown(te_fsm_state new_state, smtp_worker_thread_state_t *thread_state,
        smtp_connection_state_t *state, command_match_t match);
void server_recv_error(te_fsm_state new_state, smtp_worker_thread_state_t *thread_state,
        smtp_connection_state_t *state, command_match_t match);
void server_recv_data(te_fsm_state new_state, smtp_worker_thread_state_t *thread_state,
        smtp_connection_state_t *state, command_match_t match);
void server_recv_enddata(te_fsm_state new_state, smtp_worker_thread_state_t *thread_state,
        smtp_connection_state_t *state, command_match_t match);
void server_recv_quit(te_fsm_state new_state, smtp_worker_thread_state_t *thread_state,
        smtp_connection_state_t *state, command_match_t match);
void server_timeout(te_fsm_state new_state, smtp_connection_state_t *state);

