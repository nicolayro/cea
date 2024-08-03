#include <stdio.h>
#include <string.h>

// Undefines main function in main.c
#define UNIT_TEST
#include "main.c"

size_t num_tests;

void test(void (*f)(void), const char *display_name)
{
    num_tests++;
    printf("    %zu %s\n", num_tests, display_name);
    f();
}

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

void lines_fill(Lines *lines) {
    Line line;
    for (size_t row = 0; row < 10; ++row) { 
        line_init(&line);
        for (size_t col = 0; col < 10; ++col) {
            line_append(&line, 'a' + col);
        }
        lines_append(lines, &line);
    }

}

void lines_dump(Lines *lines) {
    printf("==================\n");
    for (size_t i = 0; i <= lines->count; ++i) { 
        Line *l = &lines->data[i];
        printf("%2zu: %.*s (%zu)\n", i, (int) l->count, l->data, l->count);
    }
    printf("==================\n");
}

void test_lines_remove(void)
{
    Lines lines;
    lines_init(&lines);
    lines_fill(&lines);

    lines_remove(&lines, 1);

    assert(lines.count == 9 && "incorrect amount of lines");
    assert(lines.data[1].count == 10 && "incorrect line length");
    assert(lines.data[1].data[0] == 'a' && "incorrect data");
}

void test_lines_remove_multiple(void)
{
    Lines lines;
    lines_init(&lines);
    lines_fill(&lines);

    lines_remove(&lines, 1);
    lines_remove(&lines, 1);

    assert(lines.count == 8 && "incorrect amount of lines");
}

void test_lines_combine(void) 
{
    Lines lines;
    lines_init(&lines);
    lines_fill(&lines);

    lines_combine(&lines, 0, 1);
 
    assert(lines.count == 9 && "incorrect amount of lines");
    assert(lines.data[0].count == 20 && "incorrect line length");
    assert(lines.data[1].count == 10 && "incorrect line length");
}

void test_lines_combine_empty(void) 
{
    Lines lines;
    lines_init(&lines);
    lines_fill(&lines);
    lines.data[1].count = 0;

    lines_combine(&lines, 0, 1);

    assert(lines.count == 9 && "incorrect amount of lines");
    assert(lines.data[0].count == 10 && "incorrect line length");
    assert(lines.data[2].count == 10 && "incorrect line length");
    assert(lines.data[1].count == 10 && "incorrect line length");
}

void test_editor_remove_char(void)
{
    Lines lines;
    lines_init(&lines);
    lines_fill(&lines);

    Editor e = {
        .cx = 3,
        .cy = 6,
        .width = 10,
        .height = 10,
        .lines = lines,
        .mode = NORMAL,
    };

    editor_remove_char(&e);

    assert(e.lines.data[6].count == 9 && "incorrect amount of chars");
    assert(e.lines.data[6].data[3] == ('a' + 4) && "incorrect amount of chars");
}

void remove_editor_remove_line_start(void)
{
    Lines lines = {0};
    lines_init(&lines);
    lines_fill(&lines);

    Editor e = {
        .cx = 0,
        .cy = 6,
        .width = 10,
        .height = 10,
        .lines = lines,
        .mode = NORMAL,
    };

    editor_remove_char(&e);

    assert(e.lines.count == 9 && "incorrect amount of lines");
    assert(e.cy == 5 && "incorrect mouse placement");
    assert(e.cx == 10 && "incorrect mouse placement");
}

int main(void) 
{
    printf("Running tests\n");
    printf("  Line\n");
    test(test_line_insert, "line_insert");
    test(test_line_insert_multiple, "multiple line_insert");
    printf("  Lines\n");
    test(test_lines_remove, "lines_remove");
    test(test_lines_remove_multiple, "lines_remove multiple");
    test(test_lines_combine, "lines_combine");
    test(test_lines_combine_empty, "lines_combine empty second line");
    printf("  Editor\n");
    test(test_editor_remove_char, "remove char");
    test(test_editor_remove_char, "remove char");
    test(remove_editor_remove_line_start, "remove char at beginning of line");
    printf("Completed %zu tests\n", num_tests);

    return 0;
}
