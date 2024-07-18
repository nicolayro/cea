#include <stdio.h>

// man(4) console_codes
#define CLEAR()       printf("\033[2J")
#define MOVE_UP(x)    printf("\033[%dA", (x))
#define MOVE_DOWN(x)  printf("\033[%dB", (x))
#define MOVE_RIGHT(x) printf("\033[%dC", (x))
#define MOVE_LEFT(x)  printf("\033[%dD", (x))
#define MOVE_TO(x,y)  printf("\033[%d;%dH", (y), (x))

void display_colors(void)
{
    int i, j, n;
    for (i = 0; i < 11; i++) {
        for (j = 0; j < 10; j++) {
            n = 10 * i + j;
            if (n > 108)
                break;
            printf("\033[%dm %3d\033[m", n, n);
        }
        printf("\n");
    }
}

int main(void)
{
    CLEAR();
    MOVE_TO(0, 0);

    char c;
    while ((c = getchar()) != 'q') {
        switch (c) {
            case 'q':
                break;
            case 'h':
                MOVE_LEFT(1);
                break;
            case 'j':
                MOVE_DOWN(1);
                break;
            case 'k':
                MOVE_UP(1);
                break;
            case 'l':
                MOVE_RIGHT(1);
                break;
        }
    }

    return 0;
}
