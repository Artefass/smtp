#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include "../message_builder.h"


void check_message(message_builder_t *builder, const char *str)
{
    int len = strlen(str);
    message_t message = message_builder_get_message(builder);
    ck_assert_int_eq(message.len, len);
    
    if (len > 0)
        ck_assert_str_eq(message.text, str);
    
    message_free(&message);
}

void check_unparsed_buffer(message_builder_t *builder, const char *str)
{
    int len = strlen(str);
    ck_assert_int_eq(builder->unparsed_message_buffer.size, len);
    ck_assert(memcmp(builder->unparsed_message_buffer.data, str, len) == 0);
}

START_TEST(partial_message)
{
    message_builder_t builder = create_message_builder();
    message_builder_add_string(&builder, "ABC", 3);
    
    ck_assert_int_eq(message_builder_ready_message_count(&builder), 0);
    
    check_unparsed_buffer(&builder, "ABC");
    check_message(&builder, "");
}
END_TEST

START_TEST(full_message_in_one_string)
{
    message_builder_t builder = create_message_builder();
    message_builder_add_string(&builder, "EHLO\r\n", 6);
    
    ck_assert_int_eq(message_builder_ready_message_count(&builder), 1);
    
    check_unparsed_buffer(&builder, "");        
    check_message(&builder, "EHLO");
}
END_TEST

START_TEST(full_message_in_two_strings)
{
    message_builder_t builder = create_message_builder();
    message_builder_add_string(&builder, "HELLO", 5);
    message_builder_add_string(&builder, "WORLD\r\n", 7);    
    
    ck_assert_int_eq(message_builder_ready_message_count(&builder), 1);
    
    check_unparsed_buffer(&builder, "");
    check_message(&builder, "HELLOWORLD");
}
END_TEST

START_TEST(one_and_half_messsages)
{
    message_builder_t builder = create_message_builder();
    message_builder_add_string(&builder, "HELLO", 5);
    message_builder_add_string(&builder, "WORLD\r\n", 7);    
    message_builder_add_string(&builder, "TEST", 4); 
    
    ck_assert_int_eq(message_builder_ready_message_count(&builder), 1);
    
    check_unparsed_buffer(&builder, "TEST");
    check_message(&builder, "HELLOWORLD");
}
END_TEST

START_TEST(two_messages)
{
    message_builder_t builder = create_message_builder();
    message_builder_add_string(&builder, "HELLO", 5);
    message_builder_add_string(&builder, "WORLD\r\n", 7);    
    message_builder_add_string(&builder, "TEST", 4); 
    message_builder_add_string(&builder, "END\r\n", 5); 
    
    ck_assert_int_eq(message_builder_ready_message_count(&builder), 2);
    
    check_unparsed_buffer(&builder, "");
    check_message(&builder, "HELLOWORLD");
    check_message(&builder, "TESTEND");
}
END_TEST

START_TEST(multiple_messages_in_one_chunk)
{
    message_builder_t builder = create_message_builder();
    message_builder_add_string(&builder, "HELLO\r\nWORLD\r\nTHERE\r\nTEST", 25);   
    ck_assert_int_eq(message_builder_ready_message_count(&builder), 3);
    
    check_unparsed_buffer(&builder, "TEST");
    check_message(&builder, "HELLO");
    check_message(&builder, "WORLD");
    check_message(&builder, "THERE");
}
END_TEST

START_TEST(message_ends_with_caret)
{
    message_builder_t builder = create_message_builder();
    
    message_builder_add_string(&builder, "HELLO\r", 6);    
    ck_assert_int_eq(message_builder_ready_message_count(&builder), 0);    
    check_unparsed_buffer(&builder, "HELLO\r");
    check_message(&builder, "");
    
    message_builder_add_string(&builder, "\nWORLD", 6);
    ck_assert_int_eq(message_builder_ready_message_count(&builder), 1);    
    check_unparsed_buffer(&builder, "WORLD");
    check_message(&builder, "HELLO");
}
END_TEST


Suite *message_builder_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Message Builder");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, partial_message);
    tcase_add_test(tc_core, full_message_in_one_string);
    tcase_add_test(tc_core, full_message_in_two_strings);
    tcase_add_test(tc_core, one_and_half_messsages);
    tcase_add_test(tc_core, two_messages);
    tcase_add_test(tc_core, multiple_messages_in_one_chunk);
    tcase_add_test(tc_core, message_ends_with_caret);
    suite_add_tcase(s, tc_core);
    return s;
}

