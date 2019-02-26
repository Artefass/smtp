#pragma once

/**
 * @file
 * @brief Динамический вектор наподобие std::vector
 */

/**
 * Динамический вектор.
 */
typedef struct dynamic_vector_t
{
    /**
     * Указатель на данные
     */
    void *data;
    /**
     * Размер элемента
     */
    int data_size;
    /**
     * Количество элементов
     */
    int size;
    /**
     * Сколько элементов может сейчас вместиться
     */
    int capacity;
} dynamic_vector_t;


dynamic_vector_t dynamic_vector_create(int data_size, int initial_capacity);
void dynamic_vector_copy_back(dynamic_vector_t *vector, const void *data, int size);
void dynamic_vector_copy_elem_back(dynamic_vector_t *vector, const void *data);
void dynamic_vector_delete_at(dynamic_vector_t *vector, int position);
void dynamic_vector_stable_delete_at(dynamic_vector_t *vector, int position);
void dynamic_vector_reserve_at_least(dynamic_vector_t *vector, int min_capacity);
void dynamic_vector_clear(dynamic_vector_t *vector);
void dynamic_vector_close(dynamic_vector_t *vector);
