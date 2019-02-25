#pragma once

/**
 * @file
 * @brief Логгирование в программе
 */

#define LOG(...) write_to_log(__FILE__, __LINE__, __VA_ARGS__)

void write_to_log(const char *file, int line, const char *format, ...);
void log_loop(int socket, char *filename);
void send_stop_log();
void set_log_socket(int socket);
