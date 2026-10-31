#include <stdio.h>
#include <unistd.h>
#include <math.h>

// Compile:
// cc -o moving_boxes_example moving_boxes_example.c -lm

int main()
{
    for (int frame_count = 0; ; ++frame_count)
    {
        int offset1 = 2.5 + 2.5 * sin(frame_count * 0.1);
        int offset2 = 2.5 - 2.5 * sin(frame_count * 0.1);

        printf(
            "\033[2J"
            "\033[1H"
        );
        for (int l = 0; l < 10; l++)
        {
            
            printf("\033[31m");
            switch (l - offset1)
            {
                case 0:  printf(".----------."); break;
                case 1:  printf("| THE      |"); break;
                case 2:  printf("|   FIRST  |"); break;
                case 3:  printf("|      BOX |"); break;
                case 4:  printf("'----------'"); break;
                default: printf("            "); break;
            }
            printf("\033[0m");
            
            printf("    "); // Four spaces
            
            printf("\033[32m");
            switch (l - offset2)
            {
                case 0:  printf(".----------."); break;
                case 1:  printf("| THE      |"); break;
                case 2:  printf("|  SECOND  |"); break;
                case 3:  printf("|      BOX |"); break;
                case 4:  printf("'----------'"); break;
                default: printf("            "); break;
            }
            printf("\033[0m");
    
            printf("\n");
        }

        usleep(1000*1000/30);
    }
}