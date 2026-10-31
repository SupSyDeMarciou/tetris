#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <pthread.h>
#include "term.portable.h"



#define GRID_WIDTH  10
#define GRID_HEIGHT 24

#define array_len(static_array) (sizeof(static_array) / sizeof((static_array)[0]))



typedef enum rotation_style {
    NONE = 1,
    HALF = 2,
    FULL = 4
} rotation_style;

typedef struct tetromino {
    rotation_style rot_style;
    char color;
    bool shape[16];
} tetromino;

static const tetromino TETROMINOS[] = {
    {   // SQUARE
        .rot_style = NONE,
        .color = 103,
        .shape = {
            0, 0, 0, 0,
            0, 1, 1, 0,
            0, 1, 1, 0,
            0, 0, 0, 0,
        }
    },
    {   // SQUIGGLY_PIECE 
        .rot_style = HALF,
        .color = 42,
        .shape = {
            0, 0, 0, 0,
            0, 0, 1, 1,
            0, 1, 1, 0,
            0, 0, 0, 0,
        }
    },
    {   // REVERSE_SQUIGGLY_PIECE 
        .rot_style = HALF,
        .color = 41,
        .shape = {
            0, 0, 0, 0,
            0, 1, 1, 0,
            0, 0, 1, 1,
            0, 0, 0, 0,
        }
    },
    {   // L_PIECE 
        .rot_style = FULL,
        .color = 43,
        .shape = {
            0, 1, 0, 0,
            0, 1, 0, 0,
            0, 1, 1, 0,
            0, 0, 0, 0,
        }
    },
    {   // REVERSE_L_PIECE 
        .rot_style = FULL,
        .color = 44,
        .shape = {
            0, 0, 1, 0,
            0, 0, 1, 0,
            0, 1, 1, 0,
            0, 0, 0, 0,
        }
    },
    {   // T_PIECE
        .rot_style = FULL,
        .color = 45,
        .shape = {
            0, 0, 0, 0,
            1, 1, 1, 0,
            0, 1, 0, 0,
            0, 0, 0, 0,
        }
    }, 
    {   // BAR_PIECE
        .rot_style = HALF,
        .color = 46,
        .shape = {
            0, 1, 0, 0,
            0, 1, 0, 0,
            0, 1, 0, 0,
            0, 1, 0, 0,
        }
    }
};
// Return a random tetromino from the above array
const tetromino *random_tetromino()
{
    // This function could be improved to make repeat pieces inpossible and better distribute
    return &TETROMINOS[rand() % array_len(TETROMINOS)];
}



typedef char grid[GRID_WIDTH * GRID_HEIGHT];

typedef enum input {
    UP       = 0b0000000,
    DOWN     = 0b0000001,
    PRESSED  = 0b0000010,
    RELEASED = 0b0000100,
    UPDATED  = 0b0001000,
} input_state;

static struct tetris
{
    struct game 
    {
        const tetromino *saved_piece;
        bool already_saved_piece;
        const tetromino *next_piece;
        struct current
        {
            const tetromino *piece;
            int x, y;
            int rotation;
        } current;
        grid grid;
    
        int level_at_reset;
        int level;
        int level_lines_cleared;
        int total_lines_cleared;
        
        int score;
        int highscore;
        
        int fall_time, fall_frame;
        bool over;
    } GAME;

    struct render 
    {
        grid   grid;
        size_t pointer;
        char   buffer[2048];
    } RENDER;

    struct inputs
    {
        union {
            struct {
                input_state left, right, down, snap_down;      // Move piece
                input_state rotate_clockwise, rotate_counter;  // Rotate piece
                input_state save;                              // Save piece

                input_state select;
            };
            input_state inputs[8];
        };
        pthread_mutex_t mutex;
    } INPUTS;

} TETRIS = {
    .INPUTS.mutex = PTHREAD_MUTEX_INITIALIZER
};
// Reset the game state to start a new tetris game
void reset_game()
{
    const int highscore      = TETRIS.GAME.highscore;      // 
    const int level_at_reset = TETRIS.GAME.level_at_reset; // Just to make life easier, these are the only fields we need to save
    memset(&TETRIS.GAME, 0, sizeof(TETRIS.GAME));
    TETRIS.GAME.highscore = highscore;
    TETRIS.GAME.level_at_reset = level_at_reset;

    TETRIS.GAME.level = level_at_reset;
    TETRIS.GAME.fall_time = 16;
    TETRIS.GAME.next_piece = random_tetromino();
}

// Check if position (x, y) is in tetromino rotated by (rot)
bool is_pos_in_current_piece_rotated(int rot, int x, int y)
{
    switch (rot) {
        case 0:  return TETRIS.GAME.current.piece->shape[(x - 0) + (y - 0) * 4]; 
        case 1:  return TETRIS.GAME.current.piece->shape[(3 - y) + (x - 0) * 4]; 
        case 2:  return TETRIS.GAME.current.piece->shape[(3 - x) + (3 - y) * 4]; 
        case 3:  return TETRIS.GAME.current.piece->shape[(y - 0) + (3 - x) * 4];
        default: return false;
    }
}
// Check wether applying the transformation (nrot, nx, ny) to the current piece results in a legal position (aka. not out of screen and not on top of already placed pieces)
bool is_move_legal(int nrot, int nx, int ny)
{
    int min_x = 0;
    int max_x = GRID_WIDTH - 1;
    for (int y = ny - 2, local_y = 0; y < ny + 2; ++y, ++local_y)
    for (int x = nx - 2, local_x = 0; x < nx + 2; ++x, ++local_x) {
        if (!is_pos_in_current_piece_rotated(nrot, local_x, local_y)) continue; // If not (tetromino block) then (continue)
        if (x < 0 || x >= GRID_WIDTH) return false; // If tetromino block is (outside of screen width) then (collision has happened), the move is illegal
        if (y < 0) continue; // If tetromino block is (above screen) then (continue)
        if (y >= GRID_HEIGHT || TETRIS.GAME.grid[x + y * GRID_WIDTH]) return false; // If tetromino block is (bellow screen) or (on top of already placed block) then (collision has happened), the move is illegal
    }

    return true; // If no tetrominos block was ever (outside of screen) or (on top of already placed block) then (no collision happened), the move is legal
}
// Place the blocks from the current piece on the input grid
void place_current_piece_in_grid(grid grid)
{
    for (int y = TETRIS.GAME.current.y - 2, local_y = 0; y < TETRIS.GAME.current.y + 2; ++y, ++local_y)
    for (int x = TETRIS.GAME.current.x - 2, local_x = 0; x < TETRIS.GAME.current.x + 2; ++x, ++local_x)
    {
        if (is_pos_in_current_piece_rotated(TETRIS.GAME.current.rotation, local_x, local_y)) 
            grid[x + GRID_WIDTH * y] = TETRIS.GAME.current.piece->color;
    }
}
// Update the game for a given set of inputs
void update(int horizontal, int vertical, int rotation, bool save_piece, bool snap_down)
{
    ++TETRIS.GAME.fall_frame;
    // When fall_frame >= fall_time, we apply 'gravity' to the piece (which speeds up the fall on harder levels). 
    // This results in downwards motion regardless of wether the player pressed "down"
    if (TETRIS.GAME.fall_frame >= TETRIS.GAME.fall_time) {
        TETRIS.GAME.fall_frame = 0;
        vertical = 1;
    }

    // If there is no "current piece", it means we need to pick from the "next" slot (happens only on frame 0)
    if (!TETRIS.GAME.current.piece) goto NEXT;

    // Saving a piece may happen only once per places tetromino to avoid stalling by saving in a loop
    if (save_piece && !TETRIS.GAME.already_saved_piece)
    {
        TETRIS.GAME.already_saved_piece = true;
        const tetromino *tmp = TETRIS.GAME.current.piece;
        TETRIS.GAME.current.piece = TETRIS.GAME.saved_piece ? TETRIS.GAME.saved_piece : TETRIS.GAME.next_piece; // If no pieces were already saved, then swap with next piece, else swap with saved piece 
        TETRIS.GAME.saved_piece = tmp;
        goto RESET;
    }

    if (snap_down)
    {
        while (is_move_legal(TETRIS.GAME.current.rotation, TETRIS.GAME.current.x, TETRIS.GAME.current.y + 1)) ++TETRIS.GAME.current.y;
        goto CHECK_PLACE;
    }

    // Transformation to try to apply to the current piece
    int nx = TETRIS.GAME.current.x + horizontal, ny = TETRIS.GAME.current.y + vertical;
    int nrot = (TETRIS.GAME.current.rotation + rotation + 4) % TETRIS.GAME.current.piece->rot_style;

    // Check if piece collides with level after applying rotation and translation
    if (is_move_legal(nrot, nx, ny)) {
        TETRIS.GAME.current.rotation = nrot;
        TETRIS.GAME.current.x = nx;
        TETRIS.GAME.current.y = ny;
        return;
    }
    
    // If applying rotation failed, try only applying translation
    if (is_move_legal(TETRIS.GAME.current.rotation, nx, ny)) {
        TETRIS.GAME.current.x = nx;
        TETRIS.GAME.current.y = ny;
        return;
    }
    
    // If applying translation failed, try only applying vertical translation
    if (is_move_legal(TETRIS.GAME.current.rotation, TETRIS.GAME.current.x, ny)) {
        TETRIS.GAME.current.y = ny;
        return;
    }

CHECK_PLACE:
    // If applying any transformation failed, check if the piece cannot be placed
    if (!is_move_legal(TETRIS.GAME.current.rotation, TETRIS.GAME.current.x, TETRIS.GAME.current.y)) {
        // Should only happen when there is no more space to place a tetromino. Therefore, this constitutes as a gameover condition
        TETRIS.GAME.over = true;
        return;
    }

    ///// At this point, we know that the piece has to be placed in the grid

    // Place the piece in the level grid
    place_current_piece_in_grid(TETRIS.GAME.grid);

    // Clear any lines that might have been formed by this piece
    // We go bottom up to allow higher lines to "fall" where removed lines once were 
    int lines_cleared = 0;
    for (int y = TETRIS.GAME.current.y - 1; y < TETRIS.GAME.current.y + 3; ++y) {
        int count = 0;
        for (int x = 0; x < GRID_WIDTH; ++x) count += (TETRIS.GAME.grid[x + GRID_WIDTH * y] > 0);
        if (count == GRID_WIDTH) {
            ++lines_cleared;
            if (y) memmove(TETRIS.GAME.grid + GRID_WIDTH, TETRIS.GAME.grid, y * GRID_WIDTH * sizeof(TETRIS.GAME.grid[0]));
        }
    }
    TETRIS.GAME.level_lines_cleared += lines_cleared;
    TETRIS.GAME.total_lines_cleared += lines_cleared;

    // Increase score for lines cleared based on current level
    switch (lines_cleared) {
        case 1: TETRIS.GAME.score += 40   * (TETRIS.GAME.level + 1); break;
        case 2: TETRIS.GAME.score += 100  * (TETRIS.GAME.level + 1); break;
        case 3: TETRIS.GAME.score += 300  * (TETRIS.GAME.level + 1); break;
        case 4: TETRIS.GAME.score += 1200 * (TETRIS.GAME.level + 1); break;
    default: break;
    }

    // If we have a new highscore, update it
    if (TETRIS.GAME.highscore < TETRIS.GAME.score) TETRIS.GAME.highscore = TETRIS.GAME.score;
    
    // Finaly, check if we have cleared enough lines to go to the next level
    if (TETRIS.GAME.level_lines_cleared >= 10) {
        ++TETRIS.GAME.level;
        TETRIS.GAME.level_lines_cleared %= 10;

        // And change the speed at which the blocks fall depending on the answer
        if      (TETRIS.GAME.level < 13) TETRIS.GAME.fall_time = 16;
        else if (TETRIS.GAME.level < 16) TETRIS.GAME.fall_time = 8;
        else if (TETRIS.GAME.level < 19) TETRIS.GAME.fall_time = 4;
        else if (TETRIS.GAME.level < 29) TETRIS.GAME.fall_time = 2;
        else                           TETRIS.GAME.fall_time = 1;
    }

NEXT:
    TETRIS.GAME.current.piece = TETRIS.GAME.next_piece;
    TETRIS.GAME.next_piece = random_tetromino();
    TETRIS.GAME.already_saved_piece = false;
RESET:
    TETRIS.GAME.current.rotation = 0;
    TETRIS.GAME.current.x = GRID_WIDTH / 2 - 1;
    TETRIS.GAME.current.y = 1;

    TETRIS.GAME.fall_frame = 0; // Makes it just a smidge more consistent
}



void drawf(const char *fmt, ...) 
{
    va_list argv;
    va_start(argv, fmt);
    TETRIS.RENDER.pointer += vsnprintf(TETRIS.RENDER.buffer + TETRIS.RENDER.pointer, sizeof(TETRIS.RENDER.buffer) - TETRIS.RENDER.pointer, fmt, argv);
    va_end(argv);
}
void draws(const char *str)
{
    int len = strlen(str);
    strncpy(TETRIS.RENDER.buffer + TETRIS.RENDER.pointer, str, len);
    TETRIS.RENDER.pointer += len;
}
void drawc(char c)
{
    if (TETRIS.RENDER.pointer < sizeof(TETRIS.RENDER.buffer)) TETRIS.RENDER.buffer[TETRIS.RENDER.pointer++] = c;
}
void drawnc(int count, char c)
{
    for (int i = 0; i < count; ++i) drawc(c);
}
// Render the game output to the screen
// We make the assumption that there is a curretly selected piece anytime this function is called
void render()
{
    ///// First, we build the grid which will be render on screen (aka. the already placed blocks + the current piece)
    memcpy(TETRIS.RENDER.grid, TETRIS.GAME.grid, sizeof(grid));
    place_current_piece_in_grid(TETRIS.RENDER.grid);
    
    ///// Then we draw the entire UI (aka. main game area + the widgets on the left, like score or 'next piece' box)
    TETRIS.RENDER.pointer = 0;
    draws("\033[1H"); // Move cursor to top left of screen

    drawc(','); drawnc(GRID_WIDTH * 2, '-'); draws(",\n"); // Draw top line
    int col = 0;
    for (int j = 0; j < GRID_HEIGHT; j++) {
        col = 0;

        // Draw main grid line: white '|', [ colored grid blocks ], white '|'
        draws("\033[0m|");
        for (int i = 0; i < GRID_WIDTH; i++) {
            if (TETRIS.RENDER.grid[i + GRID_WIDTH * j] != col) drawf("\033[%dm  ", col = TETRIS.RENDER.grid[i + GRID_WIDTH * j]);
            else                                             draws("  ");
        }
        draws("\033[0m|");

        // Switch on the line number to know what to draw
        switch (j)
        {
            // Draw "next piece" box
            case 0: draws("  ,--------,"); break;
            case 1: case 2: case 3: case 4: {
                draws("  |");
                for (int i = 0; i < 4; ++i) drawf("\033[%dm  ", TETRIS.GAME.next_piece->shape[i + (j - 1) * 4] * TETRIS.GAME.next_piece->color);
                draws("\033[0m|");
            } break;
            case 5: draws("  '--NEXT--'"); break;
        
            // Draw "saved piece" box
            case 7: draws("  ,--------,"); break;
            case 8: case 9: case 10: case 11: {
                drawf("  |");
                // We might not have a saved piece, in which case we don't want to draw anything in the box
                if (TETRIS.GAME.saved_piece) for (int i = 0; i < 4; ++i) drawf("\033[%dm  ", TETRIS.GAME.saved_piece->shape[i + (j - 8) * 4] * TETRIS.GAME.saved_piece->color);
                else                       drawnc(4 * 2, ' ');
                draws("\033[0m|");
            } break;
            case 12: draws("  '--SAVE--'"); break;

            case 14: drawf("  LINES: % 3d", TETRIS.GAME.total_lines_cleared); break;
            
            case 16: drawf("  LEVEL: % 3d", TETRIS.GAME.total_lines_cleared); break;
            
            case 18: draws("  HIGHSCORE:"); break;
            case 19: drawf("  %010d", TETRIS.GAME.highscore); break;
            
            case 21: draws("  SCORE:"); break;
            case 22: drawf("  %010d", TETRIS.GAME.score); break;

            default: break;
        }

        drawc('\n');
    }

    drawc('\''); drawnc(GRID_WIDTH * 2, '-'); drawc('\''); // Draw bottom line
    
    drawc('\0'); // We must not forget to add a null byte at the end of our render buffer to make it a valid C string before pushing to the terminal
    puts(TETRIS.RENDER.buffer);
}



// Shorthand for pthread_mutex_lock(&INPUTS.mutex) and pthread_mutex_unlock(&INPUTS.mutex)
void inputs_mutex(bool lock)
{
    (lock ? pthread_mutex_lock : pthread_mutex_unlock)(&TETRIS.INPUTS.mutex);
}
// Function executed in another thread to get user inputs
void *inputs_thread_func(void *_)
{
    while (1)
    {
        int key = inputGet();

        inputs_mutex(true);
        switch (key) {
            case 'q':          TETRIS.INPUTS.left             |= UPDATED; break;
            case 'd':          TETRIS.INPUTS.right            |= UPDATED; break;
            case 's':          TETRIS.INPUTS.down             |= UPDATED; break;
            case 'a':          TETRIS.INPUTS.save             |= UPDATED; break;
            
            case INPUT_RIGHT:  TETRIS.INPUTS.rotate_counter   |= UPDATED; break;
            case INPUT_LEFT:   TETRIS.INPUTS.rotate_clockwise |= UPDATED; break;
            case INPUT_DOWN:   TETRIS.INPUTS.snap_down        |= UPDATED; break;

            case INPUT_ENTER:  TETRIS.INPUTS.select           |= UPDATED; break;
            case INPUT_C_CTRL: printf("\033[31m[FORCED EXIT]\033[0m");    exit(0);

            case 'S':          TETRIS.INPUTS.snap_down        |= UPDATED; break;
            default:                                                      break;
        }
        inputs_mutex(false);
    }
}
// Check an input's state
bool input_pressed(input_state input)
{
    return (input & PRESSED) == PRESSED;
}
bool input_down(input_state input)
{
    return (input & DOWN) == DOWN;
}
// Update the inputs states from the main loop (allows for differenciating between key press and key hold)
void update_inputs()
{
    inputs_mutex(true);
    for (int i = 0; i < array_len(TETRIS.INPUTS.inputs); ++i)
    {
        if (TETRIS.INPUTS.inputs[i] & UPDATED) 
            switch (TETRIS.INPUTS.inputs[i] &= ~UPDATED) {
                case UP:       TETRIS.INPUTS.inputs[i] = PRESSED;  break;
                case DOWN:     TETRIS.INPUTS.inputs[i] = DOWN;     break;
                case PRESSED:  TETRIS.INPUTS.inputs[i] = DOWN;     break;
                case RELEASED: TETRIS.INPUTS.inputs[i] = UP;       break;
                default: break;
            }
        else 
            switch (TETRIS.INPUTS.inputs[i] & ~UPDATED) {
                case UP:       TETRIS.INPUTS.inputs[i] = UP;       break;
                case DOWN:     TETRIS.INPUTS.inputs[i] = RELEASED; break;
                case PRESSED:  TETRIS.INPUTS.inputs[i] = DOWN;     break;
                case RELEASED: TETRIS.INPUTS.inputs[i] = UP;       break;
                default: break;
            }
    }
    inputs_mutex(false);
}



// Wait for ENTER key to be pressed
void wait_enter()
{
    while (1)
    {
        int enter = input_pressed(TETRIS.INPUTS.select);
        update_inputs();

        if (enter) break;
        sleep(1.0 / 60.0);
    }
}
// The main game loop
void game()
{
    printf("\033[2J"); // Clear screen
    reset_game();

    while (!TETRIS.GAME.over)
    {
        inputs_mutex(true);
            int  ud   = input_down(TETRIS.INPUTS.down);
            int  lr   = input_down(TETRIS.INPUTS.right) - input_down(TETRIS.INPUTS.left);
            int  rot  = input_pressed(TETRIS.INPUTS.rotate_clockwise) - input_pressed(TETRIS.INPUTS.rotate_counter);
            bool save = input_pressed(TETRIS.INPUTS.save);
            bool snap = input_pressed(TETRIS.INPUTS.snap_down);
        inputs_mutex(false);
        update_inputs();
        
        update(lr, ud, rot, save, snap);
        render();

        sleep(1/60.0); // We want to target around 60 FPS, we guestimate that a negligeable part of the frame time is spent on processing
    }

    printf("\033[%d;%dHGAME OVER\033[100;0H< Press ENTER to continue >", 1 + GRID_HEIGHT / 2, 2 + GRID_WIDTH - (int)sizeof("GAME OVER") / 2);
    
    sleep(0.5);
    wait_enter();
}
// The main menu loop
void menu()
{
    printf(
        "\033[2J" // Clear terminal window
        "\033[1H" // Reset cursor position

        "   _____   _____   _____   ____     _____   ______  \n"
        "  |__ __| | ____| |__ __| | __  \\  |__ __| /  ___ \\ \n"
        "     | |   | |__     | |   | |_\\ |    | |   \\  \\___  \n"
        "      | |   |  __|    | |   |  _ <     | |    '--.. \\ \n"
        "       | |   | |___    | |   | | \\ \\   _| |_   _____\\ |\n"
        "        |_|   |_____|   |_|   |_|  \\_\\ |_____|  \\_____/ \n"
        "\n"
        " [Q] [D]  : Move piece left or right\n"
        "   [S]    : Move piece down\n"
        "   [↓]    : Snap piece down instantaniously\n"
        " [←] [→]  : Rotate piece clockwise or counter-clockwise\n"
        "   [A]    : Save piece\n"
        "  [^C]    : Force quit game\n"
        "\n"
        "< Press ENTER to start game >\n"
    );

    wait_enter();
}



// Load the highscore from the save file
void load_highscore()
{
    FILE* f = fopen("highscore.txt", "r");
    if (f) {
        fscanf(f, "%d", &TETRIS.GAME.highscore);
        fclose(f);
    }
}
// Save the highscore to the save file
void save_highscore()
{
    FILE *f = fopen("highscore.txt", "w");
    if (f) {
        fprintf(f, "%d", TETRIS.GAME.highscore);
        fclose(f);
    }
}

int main(int argc, char **argv)
{
    if (argc >= 2)                     TETRIS.GAME.level_at_reset = atoi(argv[1]);
    if (TETRIS.GAME.level_at_reset <= 0) TETRIS.GAME.level_at_reset = 1;

    srand(time(NULL)); // Make sure the game doesn't play out the same everytime the app is launched

    load_highscore();
    atexit(save_highscore); // Make sure the highscore is saved even when the program closes unexpectedly

    pthread_t inputs_thread;
    pthread_create(&inputs_thread, NULL, inputs_thread_func, NULL);

    puts("\033[?25l"); // Hide cursor

    while (1)
    {
        menu();
        game();
    }
}