/***************************************************************************
**                                                                        **
**                          Connect-4 Algorithm                           **
**                                                                        **
**                              Version 3.7                               **
**                                                                        **
**                            By Keith Pomakis                            **
**                          (pomakis at pobox.com)                        **
**                                                                        **
**                               May, 2000                                **
**                                                                        **
****************************************************************************
**                                                                        **
**                          Sample Implementation!                        **
**                                                                        **
**  This code is poorly written and contains no internal documentation.   **
**  Its sole purpose is to quickly demonstrate an actual implementation   **
**  of the functions contained in the file "c4.c".  It's a fully working  **
**  game which should work on any type of system, so give it a shot!      **
**                                                                        **
**  The computer is pretty brain-dead at level 3 or less, but at level 4  **
**  and up it provides quite a challenge!                                 **
**                                                                        **
****************************************************************************
**  $Id: game.c,v 3.7 2000/05/19 16:49:46 pomakis Exp pomakis $
***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "c4.h"

#define BUFFER_SIZE 80

enum {HUMAN = 0, COMPUTER = 1};

static int get_num(char *prompt, int lower, int upper, int default_val);
static void print_board(int width, int height);
static void print_dot(void);

static char piece[2] = { 'X', 'O' };

int
main()
{
    int player[2], level[2], turn = 0, num_of_players, move;
    int width, height, num_to_connect;
    int x1, y1, x2, y2;
    char buffer[BUFFER_SIZE];

    printf("\n****  Welcome to the game of Connect!  ****\n\n");
    printf("By Keith Pomakis (pomakis at pobox.com)\n");
    printf("April, 1998\n\n");

    width = get_num("Width of board", 1, 40, 7);
    height = get_num("Height of board", 1, 40, 6);
    num_to_connect = get_num("Number to connect", 1, 40, 4);

    num_of_players = get_num("Number of human players (0, 1 or 2)", 0, 2, 1);
    switch (num_of_players) {

        case 0:
            player[0] = player[1] = COMPUTER;
            level[0] = get_num("Skill level of player X", 1, C4_MAX_LEVEL, 5);
            level[1] = get_num("Skill level of player O", 1, C4_MAX_LEVEL, 5);
            turn = 0;
            break;

        case 1:
            player[0] = HUMAN;
            player[1] = COMPUTER;
            level[1] = get_num("Skill level of computer", 1, C4_MAX_LEVEL, 5);
            buffer[0] = '\0';
            do {
                printf("Would you like to go first [y]? ");
                if (!fgets(buffer, BUFFER_SIZE, stdin)) {
                    printf("\nGoodbye!\n");
                    exit(0);
                }
                buffer[0] = tolower(buffer[0]);
            } while (buffer[0] != 'y' && buffer[0] != 'n' && buffer[0] != '\n');

            turn = (buffer[0] == 'n')? 1 : 0;
            break;

        case 2:
            player[0] = player[1] = HUMAN;
            turn = 0;
            break;
    }

    c4_new_game(width, height, num_to_connect);
    c4_poll(print_dot, CLOCKS_PER_SEC/2);

    do {
        print_board(width, height);
        if (player[turn] == HUMAN) {
            do {
                if (num_of_players == 2)
                    sprintf(buffer, "Player %c, drop in which column",
                            piece[turn]);
                else
                    sprintf(buffer, "Drop in which column");
                move = get_num(buffer, 1, width, -1) - 1;
            }
            while (!c4_make_move(turn, move, NULL));
        }
        else {
            if (num_of_players == 1)
                printf("Thinking.");
            else
                printf("Player %c is thinking.", piece[turn]);
            fflush(stdout);
            c4_auto_move(turn, level[turn], &move, NULL);
            if (num_of_players == 1)
                printf("\n\nI dropped my piece into column %d.\n", move+1);
            else
                printf("\n\nPlayer %c dropped its piece into column %d.\n",
                       piece[turn], move+1);
        }

        turn = !turn;

    } while (!c4_is_winner(0) && !c4_is_winner(1) && !c4_is_tie());

    print_board(width, height);

    if (c4_is_winner(0)) {
        if (num_of_players == 1)
            printf("You won!");
        else
            printf("Player %c won!", piece[0]);
        c4_win_coords(&x1, &y1, &x2, &y2);
        printf("  (%d,%d) to (%d,%d)\n\n", x1+1, y1+1, x2+1, y2+1);
    }
    else if (c4_is_winner(1)) {
        if (num_of_players == 1)
            printf("I won!");
        else
            printf("Player %c won!", piece[1]);
        c4_win_coords(&x1, &y1, &x2, &y2);
        printf("  (%d,%d) to (%d,%d)\n\n", x1+1, y1+1, x2+1, y2+1);
    }
    else {
        printf("There was a tie!\n\n");
    }

    c4_end_game();
    return 0;
}


/****************************************************************************/

static int
get_num(char *prompt, int lower, int upper, int default_value)
{
    int number = -1;
    int result;
    static char numbuf[40];

    do {
        if (default_value != -1)
            printf("%s [%d]? ", prompt, default_value);
        else
            printf("%s? ", prompt);

        if (!fgets(numbuf, 40, stdin) || numbuf[0] == 'q') {
            printf("\nGoodbye!\n");
            exit(0);
        }
        result = sscanf(numbuf, "%d", &number);
    } while (result == 0 || (result != EOF && (number<lower || number>upper)));

    return (result == EOF? default_value : number);
}

/****************************************************************************/

static void
print_board(int width, int height)
{
    int x, y;
    char **board, spacing[2], dashing[2];

    board = c4_board();

    spacing[1] = dashing[1] = '\0';
    if (width > 19) {
        spacing[0] = '\0';
        dashing[0] = '\0';
    }
    else {
        spacing[0] = ' ';
        dashing[0] = '-';
    }

    printf("\n");
    for (y=height-1; y>=0; y--) {

        printf("|");
        for (x=0; x<width; x++) {
            if (board[x][y] == C4_NONE)
                printf("%s %s|", spacing, spacing);
            else
                printf("%s%c%s|", spacing, piece[(int)board[x][y]], spacing);
        }
        printf("\n");

        printf("+");
        for (x=0; x<width; x++)
            printf("%s-%s+", dashing, dashing);
        printf("\n");
    }

    printf(" ");
    for (x=0; x<width; x++)
        printf("%s%d%s ", spacing, (x>8)?(x+1)/10:x+1, spacing);
    if (width > 9) {
        printf("\n ");
        for (x=0; x<width; x++)
            printf("%s%c%s ", spacing, (x>8)?'0'+(x+1)-((x+1)/10)*10:' ',
                              spacing);
    }
    printf("\n\n");
}

/****************************************************************************/

static void
print_dot(void)
{
    printf(".");
    fflush(stdout);
}
