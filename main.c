#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

struct termios org_term;

#define INIT_CAP 8

// man(4) console_codes
#define CLEAR()             printf("\033[2J")
#define CURSOR_RESET()      printf("\033[H")
#define CURSOR_MOVE_TO(x,y) printf("\033[%zu;%zuH", (y)+1, (x)+1)

#define ESCAPE 27
#define BSPACE 127

void terminal_disable_raw_mode(void) 
{
    if ((tcsetattr(STDIN_FILENO, TCSAFLUSH, &org_term)) == -1) {
        fprintf(stderr, "ERROR: Unable to reset terminal.\n");
        exit(1);
    }
}

void terminal_enable_raw_mode(void) 
{
    if ((tcgetattr(STDIN_FILENO, &org_term)) == -1) {
        fprintf(stderr, "ERROR: Unable to get terminal.\n");
        exit(1);
    }
    atexit(terminal_disable_raw_mode);
    
    struct termios term = org_term;
    term.c_lflag &= ~(ECHO | ICANON);

    if ((tcsetattr(STDIN_FILENO, TCSAFLUSH, &term)) == -1) {
        fprintf(stderr, "ERROR: Unable to set terminal to raw mode.\n");
        exit(1);
    }
}

typedef enum {
    NORMAL,
    INSERT,
} Mode;

char *mode_to_str(Mode m) {
    switch (m) {
        case NORMAL: return "NORMAL";
        case INSERT: return "INSERT";
    }
}

typedef struct {
    char *data;
    size_t count;
    size_t capacity;
} Line;

typedef struct {
    Line *data;
    size_t count;
    size_t capacity;
} Lines;

typedef struct {
    size_t cx, cy;
    size_t width, height;
    Mode mode;
    Lines lines;
} Editor;

void line_init(Line *line) {
    line->count = 0;
    line->capacity = 0;
    line->data = NULL;
}

void line_insert(Line *line, char c) 
{
    if (line->capacity < line->count + 1) {
        line->capacity = line->capacity == 0 ? INIT_CAP : line->capacity * 2;
        line->data = realloc(line->data, sizeof(char) * line->capacity);
        if (!line->data) {
            fprintf(stderr, "ERROR: Not enough memory...\n");
            exit(1);
        }
    }

    line->data[line->count++] = c;
}

void lines_init(Lines *lines) 
{
    lines->count = 0;
    lines->capacity = 0;
    lines->data = NULL;
}

void lines_insert(Lines *lines, Line *line) 
{
    if (lines->capacity < lines->count + 1) {
        lines->capacity = lines->capacity == 0 ? INIT_CAP: lines->capacity * 2;
        lines->data = realloc(lines->data, sizeof(Line) * lines->capacity);
        if (!lines->data) {
            fprintf(stderr, "ERROR: Not enough memory...\n");
            exit(1);
        }
    }

    lines->data[lines->count++] = *line;
}

void lines_free(Lines *lines) 
{
    for (size_t i = 0; i < lines->count; ++i) {
        free(lines->data[i].data);
    }
    free(lines->data);
}

void editor_compute_size(Editor *e)
{
    struct winsize w;
    if ((ioctl(STDOUT_FILENO, TIOCGWINSZ, &w)) == -1) {
        fprintf(stderr, "ERROR: Unable to get terminal window size.\n");
        exit(1);
    }

    e->width = w.ws_col;
    e->height = w.ws_row;
}

void render(FILE *out, Editor *e, char last) 
{
    CLEAR();
    CURSOR_RESET();
    
    for (size_t i = 0; i < e->lines.count; ++i) {
        fprintf(out, "\033[33m%2zu\033[0m ", i);

        Line *line = &e->lines.data[i];
        fprintf(out, "%.*s\n", (int) line->count, line->data);
    }

    // status bar
    char *mode_str = mode_to_str(e->mode);
    CURSOR_MOVE_TO((size_t) 0, (size_t) e->height);
    fprintf(out, "\033[37;44m");
    fprintf(out, " | %s | %zu, %zu | %c (%d) |", mode_str, e->cx, e->cy, last, last);
    fprintf(out, "\033[0m");

    CURSOR_MOVE_TO(e->cx, e->cy);
    fflush(out);
}

int main(void)
{

    Editor e = {0};
    editor_compute_size(&e);

    Line title = {0};
    line_init(&title);

    Line msg = {0};
    line_init(&msg);

    char *title_content = "    Welcome to cea v0.1!";
    char *msg_content =   "    Begin navigating with 'hjkl'";

    for (size_t i = 0; i < strlen(title_content); ++i) {
        line_insert(&title, title_content[i]);
    }
    for (size_t i = 0; i < strlen(msg_content); ++i) {
        line_insert(&msg, msg_content[i]);
    }

    lines_init(&e.lines);
    lines_insert(&e.lines, &title);
    lines_insert(&e.lines, &msg);

    CLEAR();

    terminal_enable_raw_mode();
    render(stdout, &e, ' ');

    int c;
    while (read(STDIN_FILENO, &c, 1) == 1 && c != 'q') {
        if (e.mode == NORMAL) {
            switch (c) {
                case 'h':
                    if (e.cx > 0)
                        e.cx--;
                    break;
                case 'j':
                    if (e.cy < e.height)
                        e.cy++;
                    break;
                case 'k':
                    if (e.cy > 0)
                        e.cy--;
                    break;
                case 'l':
                    if (e.cx < e.width)
                        e.cx++;
                    break;
                case 'i':
                    e.mode = INSERT;
                    break;
                case ESCAPE:
                    e.mode = NORMAL;
                    break;
                default:
                    break;
            }
        } else if (e.mode == INSERT) {
            switch (c) {
                case ESCAPE:
                    e.mode = NORMAL;
                    break;
                case BSPACE:
                    e.cx--;
                    /* e.lines[e.cy][e.cx-3] = 0; */
                    break;
                default:
                    if (e.cy < e.lines.count)
                        line_insert(&e.lines.data[e.cy], c);
                        e.cx++;
                    break;
            }
        }

        render(stdout, &e, c);
    }

    terminal_disable_raw_mode();

    return 0;
}
