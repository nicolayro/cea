#include <stdio.h>
#include <string.h>

// Undefines main function in main.c
#define UNIT_TEST
#include "main.c"

size_t num_tests;

void test_line_insert(void)
{
    Line line;
    line_init(&line);

    line_insert(&line, 0, 'c');

    assert(line.count == 1 && "line_insert gave incorrect count");
    assert(line.data[0] == 'c' && "line_insert did not insert correct char");
}

void test_line_insert_multiple(void)
{
    Line line;
    line_init(&line);

    line_insert(&line, 0, 'a');
    line_insert(&line, 0, 'e');
    line_insert(&line, 0, 'c');
    line_insert(&line, 0, '\0');

    assert(line.count == 4 && "line_insert_multiple gave incorrect count");
    assert(strcmp(line.data, "cea") != 1 && "line_insert_multiple did not insert correct chars");
}

void test(void (*f)(void), const char *display_name)
{
    num_tests++;
    printf("    %zu %s\n", num_tests, display_name);
    f();
}

int main(void) 
{
    printf("Running tests\n");
    test(test_line_insert, "line_insert");
    test(test_line_insert_multiple, "multiple line_insert");
    printf("Completed %zu tests\n", num_tests);

    return 0;
}
