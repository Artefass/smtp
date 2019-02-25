#pragma once

#include <stdbool.h>

typedef struct dynamic_vector_t {
    /**
     * на сколько заполнен массив
     */
    int size;

    /**
     * текущая вместимость массива
     */
    int capasity;

    /**
     * массив данных
     */
    void *data;

    /**
     * размер одного элемента
     */
    int data_size;
} dynamic_vector_t;

dynamic_vector_t create_dynamic_vector(int data_size, int start_capasity);
bool increase_dynamic_vector(dynamic_vector_t *vector, int new_capasity);
bool copy_element_back_dynamic_vector(dynamic_vector_t *vector, const void *element);
bool copy_back_dynamic_vector(dynamic_vector_t *vector, const void *data, int size);
void simple_delete_element_from_dynamic_vector(dynamic_vector_t *vector, int position);
void hard_delete_element_from_dynamic_vector(dynamic_vector_t *vector, int position);
void clean_dynamic_vector(dynamic_vector_t *vector);
void close_dynamic_vector(dynamic_vector_t *vector);