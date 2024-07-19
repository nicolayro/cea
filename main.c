#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

struct termios org_term;

#define WIDTH 100
#define HEIGHT 32
// man(4) console_codes
#define CLEAR()             printf("\033[2J")
#define CURSOR_RESET()      printf("\033[H")
#define CURSOR_MOVE_TO(x,y) printf("\033[%zu;%zuH", (y), (x))

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

typedef char Line[WIDTH];

typedef struct {
    size_t cx, cy;
    size_t width, height;
    Mode mode;
    Line lines[HEIGHT];
} Editor;

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
    CURSOR_MOVE_TO((size_t) 1, (size_t) 1);
    
    for (int i = 1; i < HEIGHT; ++i) {
        fprintf(out, "\033[33m%2d\033[0m ", i);
        for (int j = 1; j < WIDTH; ++j) {
            fprintf(out, "%c", e->lines[i][j] ? e->lines[i][j] : ' ');
        }
        fprintf(out, "\n");
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
                case 'x':
                    e.lines[e.cy][e.cx-3] = 0;
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
                    e.lines[e.cy][e.cx-3] = 0;
                    break;
                default:
                    e.lines[e.cy][e.cx-3] = c;
                    e.cx++;
                    break;
            }
        }

        render(stdout, &e, c);
    }

    terminal_disable_raw_mode();

    return 0;
}
