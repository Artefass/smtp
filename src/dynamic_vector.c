#include "dynamic_vector.h" 
#include <stdlib.h>
#include <string.h>
#include <assert.h>

dynamic_vector_t dynamic_vector_create(int data_size, int initial_capacity)
{
    dynamic_vector_t vector = {};
    vector.data_size = data_size;
    vector.capacity = initial_capacity;
    vector.size = 0;
    vector.data = malloc(data_size * initial_capacity);

    return vector;
}

void dynamic_vector_copy_elem_back(dynamic_vector_t *vector, const void *data)
{
    dynamic_vector_copy_back(vector, data, 1);
}

void dynamic_vector_copy_back(dynamic_vector_t *vector, const void *data, int size)
{
    int vec_capacity = vector->capacity;
    while (vec_capacity-size < size)
    {
        vec_capacity *= 2;
    }
    dynamic_vector_reserve_at_least(vector, vec_capacity);
    memcpy(vector->data + vector->data_size * vector->size, data, vector->data_size * size);
    vector->size += size;
}

void dynamic_vector_delete_at(dynamic_vector_t *vector, int position)
{
    assert(position >= 0 && position < vector->size);

    vector->size -= 1;
    if (position != vector->size)
    {
        // Переставляем последний элемент на место удалённого
        memcpy(vector->data + vector->data_size * position, 
                vector->data + vector->data_size * vector->size, 
                vector->data_size);
    }
}

void dynamic_vector_stable_delete_at(dynamic_vector_t *vector, int position)
{
    assert(position >= 0 && position < vector->size);
    
    vector->size -= 1;
    if (position != vector->size)
    {
        int tail_size = vector->size - position;
        memmove(vector->data + vector->data_size * position,
                vector->data + vector->data_size * (position + 1),
                vector->data_size * tail_size);
    }
}

void dynamic_vector_reserve_at_least(dynamic_vector_t *vector, int min_capacity)
{
    if (vector->capacity >= min_capacity) return;

    vector->capacity = min_capacity;
    vector->data = realloc(vector->data, vector->capacity * vector->data_size);
}

void dynamic_vector_clear(dynamic_vector_t *vector)
{
    vector->size = 0;
} 

void dynamic_vector_close(dynamic_vector_t *vector)
{
    free(vector->data);
    vector->data = NULL;
    vector->capacity = 0;
    vector->data_size = 0;
    vector->size = 0;
}

