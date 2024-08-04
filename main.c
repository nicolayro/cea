#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

struct termios org_term;

#define INIT_CAP 8

// UI
#define SIDEBAR_SZ 5
#define STATUS_SZ 2

// Settings
#define TAB_SIZE 4

// Colors
#define FG_COLOR       "38;5;15"
#define BG_COLOR       "48;5;235"
#define HL_COLOR       "1;33"
#define LINE_NUM_COLOR "38;5;245"
#define PAD_COLOR      "48;5;234"

// man(4) console_codes
#define CLEAR()             printf("\033[2J")
#define CURSOR_RESET()      printf("\033[H")
#define CURSOR_MOVE_TO(x,y) printf("\033[%zu;%zuH", (y)+1, (x)+1)

// ASCII Codes
#define TAB    9
#define ENTER  10
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
    term.c_lflag &= ~(ECHO | ICANON | ISIG);

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
    size_t top, left;
    size_t height, width;
    size_t count;
    size_t capacity;
    char *content;
} Viewport;

typedef struct {
    size_t cx, cy;
    size_t width, height;
    Mode mode;
    Lines lines;
    const char *filename;
} Editor;

void viewport_insert(Viewport *v, char c) {
    if (v->capacity < v->count + 1) {
        v->capacity = v->capacity == 0 ? INIT_CAP : v->capacity * 2;
        v->content = realloc(v->content, sizeof(char) * v->capacity);
        if (!v->content) {
            fprintf(stderr, "ERROR: Not enough memory...\n");
            exit(1);
        }
    }

    v->content[v->count++] = c;
}

void viewport_write(Viewport *v, Lines *lines)
{
    v->count = 0;
    for (size_t i = v->top; i < v->top + v->height && i < lines->count; ++i) {
        Line *line = &lines->data[i];
        for (size_t j = v->left; j < v->left + v->width && j < line->count; ++j) {
            viewport_insert(v, line->data[j]);
        }
        viewport_insert(v, '\033');
        viewport_insert(v, '[');
        viewport_insert(v, 'K');
        viewport_insert(v, '\n');
    }
}

void viewport_update(Viewport *v, Editor *e)
{
    v->width = e->width - SIDEBAR_SZ;
    v->height = e->height - STATUS_SZ;

    if (e->cx <= v->left) {
        v->left = e->cx;
    }
    if (e->cx >= v->left + v->width - 1) {
        v->left = e->cx - v->width + 1;
    }
    if (e->cy <= v->top) {
        v->top = e->cy;
    }
    if (e->cy >= v->top + v->height - 1) {
        v->top = e->cy - v->height + 1;
    }

    viewport_write(v, &e->lines);
}

void viewport_free(Viewport *v)
{
    if (v->content) {
        free(v->content);
        v->count = 0;
        v->capacity = 0;
        v->content = NULL;
    }
}

void line_init(Line *line)
{
    line->count = 0;
    line->capacity = 0;
    line->data = NULL;
}

void line_append(Line *line, char c)
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

void line_insert(Line *line, size_t pos, char c)
{
    if (pos > line->count) {
        fprintf(stderr, "ERROR: Insert '%zu' out of bounds.\n", pos);
        exit(1);
    }

    if (line->capacity < line->count + 1) {
        line->capacity = line->capacity == 0 ? INIT_CAP : line->capacity * 2;
        line->data = realloc(line->data, sizeof(char) * line->capacity);
        if (!line->data) {
            fprintf(stderr, "ERROR: Not enough memory...\n");
            exit(1);
        }
    }

    if (pos == line->count) {
        line->data[line->count++] = c;
    } else {
        memmove(line->data + pos + 1, line->data + pos, line->count - pos - 1);
        line->data[pos] = c;
        line->count++;
    }
}

void line_remove(Line *line, size_t pos)
{
    if (pos >= line->count) {
        fprintf(stderr, "ERROR: Remove '%zu' out of bounds", pos);
        exit(1);
    }

    memmove(line->data + pos, line->data + pos + 1, line->count -pos - 1);
    line->count--;
}

Line line_split_at(Line *line, size_t pos)
{
    if (pos > line->count) {
        fprintf(stderr, "ERROR: Split line at '%zu' out of bounds '%zu'.\n", pos, line->count);
        exit(1);
    }

    size_t len = line->count - pos;
    Line new_line = {
        .count = len,
        .capacity = len,
        .data = malloc(sizeof(char) * len),
    };
    if (!new_line.data) {
        fprintf(stderr, "ERROR: Not enough memory...\n");
        exit(1);
    }

    memmove(new_line.data, line->data + pos, len);
    line->count = pos;

    return new_line;
}

void line_free(Line *line)
{
    if (line->data) {
        free(line->data);
        line->count = 0;
        line->capacity = 0;
        line->data = NULL;
    }
}

void lines_init(Lines *lines)
{
    lines->count = 0;
    lines->capacity = 0;
    lines->data = NULL;
}

void lines_append(Lines *lines, Line *line)
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

void lines_insert(Lines *lines, size_t pos, Line *line)
{
    if (pos > lines->count) {
        fprintf(stderr, "ERROR: Insert '%zu' out of bounds.\n", pos);
        exit(1);
    }

    if (lines->capacity < lines->count + 1) {
        lines->capacity = lines->capacity == 0 ? INIT_CAP : lines->capacity * 2;
        lines->data = realloc(lines->data, sizeof(Line) * lines->capacity);
        if (!lines->data) {
            fprintf(stderr, "ERROR: Not enough memory...\n");
            exit(1);
        }
    }

    if (pos == lines->count) {
        lines->data[lines->count++] = *line;
    } else {
        memmove(lines->data + pos + 1, lines->data + pos, sizeof(Line) * (lines->count - pos));
        lines->data[pos+1] = *line;
        lines->count++;
    }
}

void lines_remove(Lines *lines, size_t pos)
{
    if (pos >= lines->count) {
        fprintf(stderr, "ERROR: Remove '%zu' out of bounds '%zu'.\n", pos, lines->count);
        exit(1);
    }

    Line *line = &lines->data[pos];
    line_free(line);
    memmove(line, line + 1, sizeof(Line) * (lines->count - pos));
    lines->count--;
}

void lines_combine(Lines *lines, size_t pos_a, size_t pos_b)
{
    if (pos_a >= lines->count || pos_b >= lines->count) {
        fprintf(stderr, "ERROR: Combine ('%zu', '%zu') out of bounds", pos_a, pos_b);
        exit(1);
    }

    Line *a = &lines->data[pos_a];
    Line *b = &lines->data[pos_b];

    size_t count = a->count + b->count;
    if (count > a->capacity) {
        a->capacity = count;
        a->data = realloc(a->data, sizeof(Line) * a->capacity);
        if (!a->data) {
            fprintf(stderr, "ERROR: Not enough memory...\n");
            exit(1);
        }
    }

    memcpy(a->data + a->count, b->data, b->count);
    a->count += b->count;
    lines_remove(lines, pos_b);
}

void lines_free(Lines *lines)
{
    if (lines && lines->data) {
        for (size_t i = 0; i < lines->count; ++i) {
            line_free(&lines->data[i]);
        }
        free(lines->data);
        lines->count = 0;
        lines->capacity = 0;
        lines->data = NULL;
    }
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

void editor_read_from_file(Editor *e, const char* filename)
{
    struct stat statbuf;
    if ((stat(filename, &statbuf)) < 0) {
        fprintf(stderr, "ERROR: Unable to locate file '%s'.\n", filename);
        exit(1);
    }

    size_t file_size = statbuf.st_size;

    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "ERROR: Unable to open file '%s'.\n", filename);
        exit(1);
    }

    char *contents = malloc(file_size);
    if (!contents) {
        fprintf(stderr, "ERROR: Unable to load file '%s' to memory.\n", filename);
        exit(1);
    }

    size_t bytes_read = fread(contents, sizeof(char), file_size/sizeof(char), file);
    if (bytes_read < file_size) {
        fprintf(stderr, "ERROR: Only %zu bytes of %zu were read.\n", bytes_read, file_size);
        exit(1);
    }

    Line l;
    line_init(&l);
    char c;
    for (size_t i = 0; i < file_size; ++i) {
        c = contents[i];
        if (c == '\n') {
            lines_append(&e->lines, &l);
            line_init(&l);
        } else {
            line_append(&l, c);
        }
    }
    e->filename = filename;

    free(contents);
    fclose(file);
}

void editor_save_to_file(Editor *e, const char *filename) 
{
    FILE *file = fopen(filename, "w");
    if (!file) {
        fprintf(stderr, "ERROR: Unable to open file '%s' for saving.\n", filename);
        exit(1);
    }

    for (size_t i = 0; i < e->lines.count; ++i) {
        Line *line = &e->lines.data[i];
        fprintf(file, "%.*s\n", (int) line->count, line->data);
    }

    fclose(file);
}

void editor_remove_char(Editor *e)
{
    if (e->cy < e->lines.count) {
        if (e->cx == 0) {
            if (e->cy < 1)
                return;
            size_t line_end = e->lines.data[e->cy-1].count;
            lines_combine(&e->lines, e->cy-1, e->cy);
            e->cy--;
            e->cx = line_end;
        } else {
            Line *line = &e->lines.data[e->cy];
            if (e->cx <= line->count) {
                e->cx--;
                line_remove(line, e->cx);
            }
        }
    }
}

void editor_free(Editor *e)
{
    lines_free(&e->lines);
}

void render(FILE *out, Editor *e, Viewport *v, char last)
{
    size_t
        i = 0,
        line_number = 0,
        line_len = 0;

    fprintf(out, "\033["BG_COLOR"m");
    for (i = 0; i < v->count; ++i) {
        if (v->content[i] == '\n') {
            fprintf(out, "\033[%zu;%dH\033[%sm%4zu \033[22;"FG_COLOR"m",
                    line_number + 1,
                    1,
                    e->cy == line_number + v->top ? HL_COLOR : LINE_NUM_COLOR,
                    line_number + v->top + 1);
            fwrite(v->content + i - line_len, sizeof(char), line_len, out);
            line_len = 0;
            line_number++;
        } else {
            line_len++;
        }
    }

    for (;line_number < v->top + v->height; line_number++) {
        fprintf(out, "\033[%zu;%dH\033["LINE_NUM_COLOR";"PAD_COLOR"m~\033[K", line_number + 1, 1);
    }

    CURSOR_MOVE_TO((size_t) 0, v->top + v->height);
    fprintf(out, "\033[1;30;42m | %s | (%zu, %zu) | [%zu, %zu] | {%zu, %zu} | %d |\033[K\033[22m", 
            mode_to_str(e->mode), e->cx, e->cy, e->width, e->height, v->left, v->top, last);
    CURSOR_MOVE_TO((size_t) 0, v->top + v->height + 1);
    fprintf(out, "\033["BG_COLOR"m\033[K");

    CURSOR_MOVE_TO(e->cx + SIDEBAR_SZ - v->left, e->cy - v->top);
    fflush(out);
}

void run(Editor *e, Viewport *v)
{
    int c;
    while (read(STDIN_FILENO, &c, 1) == 1 && c != 'q') {
        if (e->mode == NORMAL) {
            switch (c) {
                case 'h':
                    if (e->cx > 0)
                        e->cx--;
                    break;
                case 'j':
                    if (e->cy < e->lines.count - 1)
                        e->cy++;
                    break;
                case 'k':
                    if (e->cy > 0)
                        e->cy--;
                    break;
                case 'l':
                    if (e->cx < e->lines.data[e->cy].count - 1) {
                        e->cx++;
                    }
                    break;
                case 'i':
                    if (e->cy < e->lines.count && e->cx <= e->lines.data[e->cy].count)
                        e->mode = INSERT;
                    break;
                case 'a':
                    if (e->cy < e->lines.count && e->cx < e->lines.data[e->cy].count) {
                        e->mode = INSERT;
                        e->cx++;
                    }
                    break;
                case 'A':
                    if (e->cy < e->lines.count) {
                        Line *line = &e->lines.data[e->cy];
                        if (e->cx < line->count) {
                            e->mode = INSERT;
                            e->cx = line->count;
                        }
                    }
                    break;
                case 'o':
                    if (e->cy < e->lines.count) {
                        Line line = {0};

                        lines_insert(&e->lines, e->cy, &line);
                        e->cx = 0;
                        e->cy++;
                        e->mode = INSERT;
                    }
                    break;
                case 's':
                    CURSOR_MOVE_TO((size_t) 0, v->top + v->height + 1);
                    fprintf(stdout, "\033["HL_COLOR"mSave buffer to '%s'? \033[0m", e->filename);
                    int yn = getchar();
                    if (yn == 'y') {
                        editor_save_to_file(e, e->filename);
                    }
                    fprintf(stdout, "\033["LINE_NUM_COLOR"mSave buffer to '%s'? \033[0m", e->filename);
                    break;
                case 'x':
                    if (e->cy < e->lines.count) {
                        Line *line = &e->lines.data[e->cy];
                        if (e->cx < line->count) {
                            line_remove(line, e->cx);
                        }
                    }
                    break;
                default:
                    break;
            }
        } else if (e->mode == INSERT) {
            switch (c) {
                case ESCAPE:
                    e->mode = NORMAL;
                    if (e->cx > 0) {
                        e->cx--;
                    }
                    break;
                case ENTER:
                    if (e->cy < e->lines.count) {
                        Line *line = &e->lines.data[e->cy];
                        if (e->cx >= 0 && e->cy <= e->lines.count) {
                            Line new_line = line->count != 0
                                ? line_split_at(line, e->cx)
                                : (Line) {0};
                            lines_insert(&e->lines, e->cy, &new_line);
                            e->cy++;
                            e->cx = 0;
                        }
                    }
                    break;
                case BSPACE:
                    editor_remove_char(e);
                    break;
                case TAB:
                    if (e->cy < e->lines.count) {
                        Line *line = &e->lines.data[e->cy];
                        size_t tab_size = TAB_SIZE - e->cx % TAB_SIZE;
                        for (size_t i = 0; i < tab_size; ++i) {
                            line_insert(line, e->cx, ' ');
                            e->cx++;
                        }
                    }
                default:
                    if (c >= 32 && c <= 127) {
                        if (e->cy < e->lines.count) {
                            Line *line = &e->lines.data[e->cy];
                            if (e->cx <= line->count) {
                                line_insert(line, e->cx, c);
                                e->cx++;
                            }
                        }
                    }
                    break;
            }
        }

        viewport_update(v, e);
        render(stdout, e, v, c);
    }
}

#ifndef UNIT_TEST
int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "Invalid number of arguments provided.\n");
        fprintf(stdout, "\nUSAGE: cea <filename>\n");
        exit(1);
    }

    char *filename = argv[1];

    Editor e = {0};
    Viewport v = {0};

    editor_read_from_file(&e, filename);
    editor_compute_size(&e);

    viewport_update(&v, &e);
    viewport_write(&v, &e.lines);

    CLEAR();
    terminal_enable_raw_mode();

    render(stdout, &e, &v, ' ');
    run(&e, &v);

    terminal_disable_raw_mode();

    editor_free(&e);
    viewport_free(&v);

    return 0;
}
#endif
