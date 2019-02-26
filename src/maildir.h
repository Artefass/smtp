#pragma once

#include <dirent.h>
#include <errno.h>
#include <time.h>

/**
 * @file 
 * @brief Структуры и функции для работы с maildir.
 */

/**
 * @defgroup Maildirgroup Maildir
 * @{
 */


/**
 * @brief Стркутура папки maildir
 */
typedef struct maildir_t
{
    /**
     * Корневая папка.
     */
    char *root_path;
    /**
     * Папка для временных файлов.
     */
    char *tmp_path;
    /**
     * Папка для почты, предназначенной данному серверу.
     */
    char *cur_path;
    /**
     * Папка для почты, предназначенной другому хосту.
     * Впоследствии почту из этой папки должен будет 
     * подхватить SMTP-клиент и отправить дальше.
     */
    char *relay_path;
} maildir_t;

/**
 * @brief Создаёт структуру #maildir_t и инициализирует все поля
 *
 * Данная функция пытается открыть всё дерево папок в maildir.
 * Если открыть их не получилось, то функция сначала попытается создать
 * недостающие папки, и потом снова открыть их.
 *
 * Возвоащеённую структуру следует закрывать функцией #maildir_close 
 *
 * @note Путь до папки @a root_path должен существовать
 *
 * @param root_path - путь до корневой папки maildir
 * @param error - код ошибки. 0 в случае успеха
 * @return Открытую структуру #maildir_t и @c *error == 0 в случае успеха.
 * @c *error != 0 при неудаче.
 */
maildir_t maildir_open(const char *root_path, int *error);

/**
 * @brief Закрывает структуру #maildir_t
 */
void maildir_close(maildir_t *maildir);


/**
 * Генерирует случайное имя для файла почты
 */
void generate_filename(char *buf, int len,
        struct timespec time, int random_number, int pid, int worker_id, int deliveries, char *hostname);

/**
 * Создаёт файл письма в папке tmp
 */
int maildir_create_in_tmp(maildir_t *m, char *name);

/**
 * Переносит файл из tmp в cur
 */
void maildir_move_to_cur(maildir_t *m, char *filename);

/**
 * Переносит файл из tmp в relay
 */
void maildir_move_to_relay(maildir_t *m, char *filename);

/*
 * @}
 */
