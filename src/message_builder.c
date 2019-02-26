#include "message_builder.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>


message_builder_t create_message_builder()
{
    message_builder_t result;
    result.unparsed_message_buffer = dynamic_vector_create(sizeof(char), 256);
    result.parsed_message_buffer = dynamic_vector_create(sizeof(message_t), 16);

    return result;
}

void message_builder_add_string(message_builder_t *builder, const char *str, int len)
{
    assert(builder);
    assert(len > 0);

    dynamic_vector_copy_back(&builder->unparsed_message_buffer, str, len * sizeof(char));

    const char *data = builder->unparsed_message_buffer.data;
    int message_start = 0;
    for (int i = 0; i < builder->unparsed_message_buffer.size - 1; i++)
    {
        if (data[i] == '\r' && data[i + 1] == '\n')
        {
            int message_end = i+1;
            int message_len = message_end - message_start + 1 - 2; // не копируем \r\n

            char *str = malloc(message_len + 1);
            memcpy(str, data + message_start, message_len);
            str[message_len] = '\0';

            message_t message = { .len = message_len, .text = str };
            dynamic_vector_copy_elem_back(&builder->parsed_message_buffer, &message);

            message_start = i + 2;
            i++;
        }
    }

    int unparsed_data_left = builder->unparsed_message_buffer.size - message_start;
    memmove(builder->unparsed_message_buffer.data,
            builder->unparsed_message_buffer.data + message_start,
            unparsed_data_left);
    builder->unparsed_message_buffer.size = unparsed_data_left;

}

void message_builder_close(message_builder_t *builder)
{
    dynamic_vector_close(&builder->unparsed_message_buffer);
    dynamic_vector_close(&builder->parsed_message_buffer);
}

int message_builder_ready_message_count(message_builder_t *builder)
{
    return builder->parsed_message_buffer.size;
}

// TODO: отдавать все сообщения, а не по одному? Позволит избежать
// лишних операций с буфером сообщений. Возможно стоит использовать
// кольцевой буфер вместо линейного
message_t message_builder_get_message(message_builder_t *builder)
{
    if (builder->parsed_message_buffer.size < 1)
    {
        return (message_t){0,0};
    }

    message_t result = *((message_t*)builder->parsed_message_buffer.data);
    // Используем стабильное удаление, так как нам важен порядок элементов
    dynamic_vector_stable_delete_at(&builder->parsed_message_buffer, 0);

    return result;
}


void message_free(message_t *message)
{
    free(message->text);
}
