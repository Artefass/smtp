#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../commands.h"


char buf[512];

message_t create_message(const char *text)
{
    return (message_t)
    {
        .text = strdup(text),
        .len = strlen(text),
    };
}


void assert_message(const char *messageText, enum smtp_client_command expected_command,
                    const char *capture_name, const char *expected_str)
{
    bool init = client_command_parser_init();
    ck_assert(init);
    message_t msg = create_message(messageText);
    
    command_match_t match = client_command_parse_message(&msg);
    
    ck_assert_int_eq(expected_command, match.command);
    
    if (expected_command == CLIENT_UNKNOWN_COMMAND) return;
    if (!capture_name || ! expected_str) return;
    ck_assert(match.match_data != NULL);
    
    char *str;
    int len;
    
    bool result = command_get_substring(&match, capture_name, &str, &len);
    
    ck_assert(result);
    ck_assert(strlen(expected_str) == (unsigned long)len);
    ck_assert(memcmp(str, expected_str, len) == 0);
    
    command_free_substring(str);
    
    free(msg.text);
}



START_TEST(helo)
{
    assert_message("HELO test", CLIENT_HELO, "domain", "test");
    assert_message("HELO foo.com", CLIENT_HELO, "domain", "foo.com");
    assert_message("HELO google.co.uk.org", CLIENT_HELO, "domain", "google.co.uk.org");
}
END_TEST

START_TEST(ehlo_domain)
{
    assert_message("EHLO test", CLIENT_EHLO, "domain", "test");
    assert_message("EHLO foo.com", CLIENT_EHLO, "domain", "foo.com");
    assert_message("EHLO google.co.uk.org", CLIENT_EHLO, "domain", "google.co.uk.org");
}
END_TEST

START_TEST(ehlo_ip)
{
    assert_message("EHLO [214.2.3.15]", CLIENT_EHLO, "ipv4", "214.2.3.15");
    assert_message("EHLO [127.0.0.1]", CLIENT_EHLO, "ipv4", "127.0.0.1");
}
END_TEST


START_TEST(ehlo_ipv6)
{
    assert_message("EHLO [IPv6:2001:0db8:0000:0042:0000:8a2e:0370:7334]", CLIENT_EHLO, "ipv6", "2001:0db8:0000:0042:0000:8a2e:0370:7334");
}
END_TEST

START_TEST(vrfy)
{
    assert_message("VRFY foo", CLIENT_VRFY, NULL, NULL);
    
    // У VRFY должен быть параметр
    assert_message("VRFY", CLIENT_UNKNOWN_COMMAND, NULL, NULL);
}
END_TEST

START_TEST(rset)
{
    assert_message("RSET", CLIENT_RSET, NULL, NULL);
    
    // У RSET не должно быть параметров
    assert_message("RSET foo", CLIENT_UNKNOWN_COMMAND, NULL, NULL);
}
END_TEST

START_TEST(quit)
{
    assert_message("QUIT", CLIENT_QUIT, NULL, NULL);
    
    // У QUIT не должно быть параметров
    assert_message("QUIT foo", CLIENT_UNKNOWN_COMMAND, NULL, NULL);
}
END_TEST

START_TEST(mailfrom)
{
    assert_message("MAIL FROM:<Smith@bar.com>", CLIENT_MAILFROM, "sender", "<Smith@bar.com>");
    assert_message("MAIL FROM:<Smith@bar.com>", CLIENT_MAILFROM, "rpath", "<Smith@bar.com>");
    assert_message("MAIL FROM:<Smith@bar.com>", CLIENT_MAILFROM, "localpart", "Smith");
    assert_message("MAIL FROM:<Smith@bar.com>", CLIENT_MAILFROM, "domain", "bar.com");
}
END_TEST

START_TEST(mailfrom_emptyreversepath)
{
    assert_message("MAIL FROM:<>", CLIENT_MAILFROM, "emptyrpath", "<>");
}
END_TEST

START_TEST(mailfrom_ip)
{
    assert_message("MAIL FROM:<Bob@[127.0.0.1]>", CLIENT_MAILFROM, "sender", "<Bob@[127.0.0.1]>");
    assert_message("MAIL FROM:<Bob@[127.0.0.1]>", CLIENT_MAILFROM, "rpath", "<Bob@[127.0.0.1]>");
    assert_message("MAIL FROM:<Bob@[127.0.0.1]>", CLIENT_MAILFROM, "localpart", "Bob");
    assert_message("MAIL FROM:<Bob@[127.0.0.1]>", CLIENT_MAILFROM, "ipv4", "127.0.0.1");
}
END_TEST

START_TEST(rcptto)
{
    assert_message("RCPT TO:<Fgsfds@gmail.com>", CLIENT_RCPTTO, "recepient", "<Fgsfds@gmail.com>");
    assert_message("RCPT TO:<Fgsfds@gmail.com>", CLIENT_RCPTTO, "fpath", "<Fgsfds@gmail.com>");
    assert_message("RCPT TO:<Fgsfds@gmail.com>", CLIENT_RCPTTO, "localpart", "Fgsfds");
    assert_message("RCPT TO:<Fgsfds@gmail.com>", CLIENT_RCPTTO, "domain", "gmail.com");
    assert_message("RCPT TO:<oleg@mysmtp.pvs.bmstu>", CLIENT_RCPTTO, "domain", "mysmtp.pvs.bmstu");    
}
END_TEST

Suite *command_parse_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Client Command Parser");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, helo);
    tcase_add_test(tc_core, ehlo_domain);
    tcase_add_test(tc_core, ehlo_ip);
    tcase_add_test(tc_core, ehlo_ipv6);
    tcase_add_test(tc_core, vrfy);
    tcase_add_test(tc_core, rset);
    tcase_add_test(tc_core, quit);
    tcase_add_test(tc_core, mailfrom);
    tcase_add_test(tc_core, mailfrom_emptyreversepath);
    tcase_add_test(tc_core, mailfrom_ip);
    tcase_add_test(tc_core, rcptto);
    suite_add_tcase(s, tc_core);
    return s;
}

