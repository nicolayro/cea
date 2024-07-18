#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

struct termios org_term;

// man(4) console_codes
#define CLEAR()       printf("\033[2J")
#define MOVE_UP(x)    printf("\033[%dA", (x))
#define MOVE_DOWN(x)  printf("\033[%dB", (x))
#define MOVE_RIGHT(x) printf("\033[%dC", (x))
#define MOVE_LEFT(x)  printf("\033[%dD", (x))
#define MOVE_TO(x,y)  printf("\033[%d;%dH", (y), (x))

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


int main(void)
{
    terminal_enable_raw_mode();

    CLEAR();
    MOVE_TO(0, 0);

    char c;
    while (read(STDIN_FILENO, &c, 1) == 1 && c != 'q') {
        printf("%d\n", c);
    }

    terminal_disable_raw_mode();

    return 0;
}
