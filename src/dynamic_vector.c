#include "dynamic_vector.h"

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

dynamic_vector_t create_dynamic_vector(int data_size, int start_capasity) 
{
    assert(start_capasity > 0);

    dynamic_vector_t vector;
    vector.data_size = data_size;
    vector.capasity = (start_capasity >= 16) ? start_capasity: 16;
    vector.size = 0;
    vector.data = malloc(vector.capasity * data_size);
    if (vector.data == NULL) 
    {
        vector.capasity = 0;
    }
    return vector;
}

bool increase_dynamic_vector(dynamic_vector_t *vector, int new_capasity)
{

    assert(new_capasity > 0);

    if (vector->capasity > new_capasity) 
        return true;

    void *data = realloc(vector->data, new_capasity * vector->data_size);
    if (!data)
    {
        return false;
    }

    vector->data = data;
    vector->capasity = new_capasity;

    return true; 
}

bool copy_element_back_dynamic_vector(dynamic_vector_t *vector, const void *element)
{
    return copy_back_dynamic_vector(vector, element, 1);
}


bool copy_back_dynamic_vector(dynamic_vector_t *vector, const void *data, int size)
{

    int ve_capasity = vector->capasity;
    while (ve_capasity - size < size) {
        ve_capasity *= 2;
    }

    if (!increase_dynamic_vector(vector, ve_capasity))
    {
        return false;
    }

    memcpy(vector->data + vector->data_size * vector->size, data, vector->data_size * size);
    vector->size += size;

    return true;
}

void simple_delete_element_from_dynamic_vector(dynamic_vector_t *vector, int position)
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


void hard_delete_element_from_dynamic_vector(dynamic_vector_t *vector, int position)
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


void clean_dynamic_vector(dynamic_vector_t *vector)
{
    vector->size = 0;
}


void close_dynamic_vector(dynamic_vector_t *vector) 
{
    if (vector->data) 
    {
        free(vector->data);
    }
    vector->data = NULL;
    vector->size = 0;
    vector->data_size = 0;
    vector->capasity = 0;
}

