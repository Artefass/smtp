#include <check.h>
#include <stdlib.h>

Suite *message_builder_suite(void);
Suite *command_parse_suite(void);

int main()
{
    int number_failed;
    Suite *s;
    SRunner *sr;
    s = message_builder_suite();
    sr = srunner_create(s);
    srunner_add_suite(sr, command_parse_suite());
    
    srunner_set_fork_status(sr, CK_NOFORK);    
    
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
