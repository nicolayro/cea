#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

struct termios org_term;

// man(4) console_codes
#define CLEAR()             printf("\033[2J")
#define CURSOR_RESET()      printf("\033[H")
#define CURSOR_MOVE_TO(x,y) printf("\033[%zu;%zuH", (y), (x))

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

typedef struct {
    size_t cx, cy;
    size_t width, height;
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

void render(FILE *out, Editor *e) 
{
    CLEAR();

    // Render status bar
    CURSOR_MOVE_TO((size_t) 0, (size_t) e->height);
    fprintf(out, "\033[37;44m");
    fprintf(out, " | INSERT | %zu, %zu | ", e->cx, e->cy);
    fprintf(out, "\033[0m");

    CURSOR_MOVE_TO(e->cx, e->cy);
    fflush(out);
}

int main(void)
{

    Editor e = {0};
    editor_compute_size(&e);

    terminal_enable_raw_mode();
    render(stdout, &e);

    int c;
    while (read(STDIN_FILENO, &c, 1) == 1 && c != 'q') {
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
            default:
                break;
        }

        render(stdout, &e);
    }

    terminal_disable_raw_mode();

    return 0;
}
