#ifndef __INPUTS_H__
#define __INPUTS_H__

#include <stdio.h>

#ifdef _WIN32
#   include <windows.h>
#   define sleep(s) _sleep(s)

#   include <conio.h>
#   define getch _getch

    /// @brief Get the size of the terminal window
    /// @param w The width of the window
    /// @param h The height of the window
    static inline void getTermSize(int* w, int* h) {
        // credits: https://stackoverflow.com/questions/6812224/getting-terminal-size-in-c-for-windows
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
        *w = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        *h = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    }
#else
#   include <unistd.h>
#   define sleep(s) usleep(1000*1000 * (s))

#   include <termios.h>
#   include <sys/ioctl.h>

    static inline int getch(void) {
        // credits: https://stackoverflow.com/questions/3276546/how-to-implement-getch-function-of-c-in-linux
        struct termios oldattr, newattr;
        tcgetattr( STDIN_FILENO, &oldattr );
        newattr = oldattr;
        newattr.c_lflag &= ~( ICANON | ECHO );
        tcsetattr( STDIN_FILENO, TCSANOW, &newattr );
        int ch = getchar();
        tcsetattr( STDIN_FILENO, TCSANOW, &oldattr );
        return ch;
    }

    /// @brief Get the size of the terminal window
    /// @param w The width of the window
    /// @param h The height of the window
    static inline void getTermSize(int* w, int* h) {
        // credits: https://stackoverflow.com/questions/1022957/getting-terminal-width-in-c
        struct winsize ws;
        ioctl(0, TIOCGWINSZ, &ws);
        *w = ws.ws_row; *h = ws.ws_col;
    }
#endif

/// @brief Some non-alphanumerical inputs we might expect
typedef enum InputKey {
#ifdef _WIN32
    INPUT_ENTER       = '\r',
    INPUT_BACKSPACE   = '\b',
    #else
    INPUT_ENTER       = '\n',
    INPUT_BACKSPACE   = 127,
#endif
    INPUT_A_CTRL      = 1,
    INPUT_C_CTRL      = 3,
    INPUT_V_CTRL      = 22,
    INPUT_X_CTRL      = 24,
    INPUT_Z_CTRL      = 26,

    ESCAPE_CODE       = 27,
    __INPUT_NON_ASCII = 256,
    INPUT_UP,
    INPUT_DOWN,
    INPUT_RIGHT,
    INPUT_LEFT,
} input_key;

/// @brief Get the last pressed key
/// @return The pressed input_key
static inline int inputGet() {
    const int base = getch();
    if (base != ESCAPE_CODE) return base;

    const int expect_sqr_bracket = getch();
    if (expect_sqr_bracket != '[') return expect_sqr_bracket;

    const int c = getch();
    switch (c) {
        case 'A': return INPUT_UP;
        case 'B': return INPUT_DOWN;
        case 'C': return INPUT_RIGHT;
        case 'D': return INPUT_LEFT;
        default:  return c;
    }
}

#endif