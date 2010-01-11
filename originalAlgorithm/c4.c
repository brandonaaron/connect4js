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
**  This file provides the functions necessary to implement a front-end-  **
**  independent Connect-4 game.  Multiple board sizes are supported.      **
**  It is also possible to specify the number of pieces necessary to      **
**  connect in a row in order to win.  Therefore one can play Connect-3,  **
**  Connect-5, etc.  An efficient tree-searching algorithm (making use    **
**  of alpha-beta cutoff decisions) has been implemented to insure that   **
**  the computer plays as quickly as possible.                            **
**                                                                        **
**  The declaration of the public functions necessary to use this file    **
**  are contained in "c4.h".                                              **
**                                                                        **
**  In all of the public functions (all of which have the "c4_" prefix),  **
**  the value of player can be any integer, where an even integer refers  **
**  to player 0 and an odd integer refers to player 1.                    **
**                                                                        **
****************************************************************************
**  $Id: c4.c,v 3.7 2000/05/19 16:49:46 pomakis Exp pomakis $
***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <assert.h>
#include "c4.h"

/* Some macros for convenience. */

#define other(x)        ((x) ^ 1)
#define real_player(x)  ((x) & 1)

#define pop_state() \
        (current_state = &state_stack[--depth])

/* The "goodness" of the current state with respect to a player is the */
/* score of that player minus the score of the player's opponent.  A   */
/* positive value will result if the specified player is in a better   */
/* situation than his/her opponent.                                    */

#define goodness_of(player) \
        (current_state->score[player] - current_state->score[other(player)])

/* A local struct which defines the state of a game. */

typedef struct {

    char **board;           /* The board configuration of the game state.  */
                            /* board[x][y] specifies the position of the   */
                            /* xth column and the yth row of the board,    */
                            /* where column and row numbering starts at 0. */
                            /* (The 0th row is the bottom row.)            */
                            /* A value of 0 specifies that the position is */
                            /* occupied by a piece owned by player 0, a    */
                            /* value of 1 specifies that the position is   */
                            /* occupied by a piece owned by player 1, and  */
                            /* a value of C4_NONE specifies that the       */
                            /* position is unoccupied.                     */

    int *(score_array[2]);  /* An array specifying statistics on both      */
                            /* players.  score_array[0] specifies the      */
                            /* statistics for player 0, while              */
                            /* score_array[1] specifies the statistics for */
                            /* player 1.                                   */

    int score[2];           /* The actual scores of each player, deducible */
                            /* from score_array, but kept separately for   */
                            /* efficiency.  The score of player x is the   */
                            /* sum of score_array[x].  A score is          */
                            /* basically a function of how many winning    */
                            /* positions are still available to the        */
                            /* and how close he/she is to achieving each   */
                            /* of these positions.                         */

    short int winner;       /* The winner of the game - either 0, 1 or     */
                            /* C4_NONE.  Deducible from score_array, but   */
                            /* kept separately for efficiency.             */

    int num_of_pieces;      /* The number of pieces currently occupying    */
                            /* board spaces.  Deducible from board, but    */
                            /* kept separately for efficiency.             */

} Game_state;

/* Static global variables. */

static int size_x, size_y, num_to_connect;
static int win_places;

static int ***map;  /* map[x][y] is an array of win place indices, */
                    /* terminated by a -1.                         */

static int magic_win_number;
static Boolean game_in_progress = FALSE, move_in_progress = FALSE;
static Boolean seed_chosen = FALSE;
static void (*poll_function)(void) = NULL;
static clock_t poll_interval, next_poll;
static Game_state state_stack[C4_MAX_LEVEL+1];
static Game_state *current_state;
static int depth;
static int states_allocated = 0;
static int *drop_order;

/* A declaration of the local functions. */

static int num_of_win_places(int x, int y, int n);
static void update_score(int player, int x, int y);
static int drop_piece(int player, int column);
static void push_state(void);
static int evaluate(int player, int level, int alpha, int beta);
static void *emalloc(unsigned int n);


/****************************************************************************/
/**                                                                        **/
/**  This function is used to specify a poll function and the interval at  **/
/**  which it should be called.  A poll function can be used, for example, **/
/**  to tend to any front-end interface tasks, such as updating graphics,  **/
/**  etc.  The specified poll function should accept void and return void. **/
/**  The interval unit is 1/CLOCKS_PER_SEC seconds of processor time.      **/
/**  Therefore, specifying CLOCKS_PER_SEC as the interval will cause the   **/
/**  poll function to be called once every second of processor time, while **/
/**  specifying CLOCKS_PER_SEC/4 will cause it to be called once every     **/
/**  1/4 second of processor time.                                         **/
/**                                                                        **/
/**  If no polling is required, the poll function can be specified as      **/
/**  NULL.  This is the default.                                           **/
/**                                                                        **/
/**  This function can be called an arbitrary number of times throughout   **/
/**  any game.                                                             **/
/**                                                                        **/
/**  It is illegal for the specified poll function to call the functions   **/
/**  c4_make_move(), c4_auto_move(), c4_end_game() or c4_reset().          **/
/**                                                                        **/
/****************************************************************************/

void
c4_poll(void (*poll_func)(void), clock_t interval)
{
    poll_function = poll_func;
    poll_interval = interval;
}


/****************************************************************************/
/**                                                                        **/
/**  This function sets up a new game.  This must be called exactly once   **/
/**  before each game is started.  Before it can be called a second time,  **/
/**  end_game() must be called to destroy the previous game.               **/
/**                                                                        **/
/**  width and height are the desired dimensions of the game board, while  **/
/**  num is the number of pieces required to connect in a row in order to  **/
/**  win the game.                                                         **/
/**                                                                        **/
/****************************************************************************/

void
c4_new_game(int width, int height, int num)
{
    register int i, j, k, x;
    int win_index, column;
    int *win_indices;

    assert(!game_in_progress);
    assert(width >= 1 && height >= 1 && num >= 1);

    size_x = width;
    size_y = height;
    num_to_connect = num;
    magic_win_number = 1 << num_to_connect;
    win_places = num_of_win_places(size_x, size_y, num_to_connect);

    /* Set up a random seed for making random decisions when there is */
    /* equal goodness between two moves.                              */

    if (!seed_chosen) {
        srand((unsigned int) time((time_t *) 0));
        seed_chosen = TRUE;
    }

    /* Set up the board */

    depth = 0;
    current_state = &state_stack[0];

    current_state->board = (char **) emalloc(size_x * sizeof(char *));
    for (i=0; i<size_x; i++) {
        current_state->board[i] = (char *) emalloc(size_y);
        for (j=0; j<size_y; j++)
            current_state->board[i][j] = C4_NONE;
    }

    /* Set up the score array */

    current_state->score_array[0] = (int *) emalloc(win_places * sizeof(int));
    current_state->score_array[1] = (int *) emalloc(win_places * sizeof(int));
    for (i=0; i<win_places; i++) {
        current_state->score_array[0][i] = 1;
        current_state->score_array[1][i] = 1;
    }

    current_state->score[0] = current_state->score[1] = win_places;
    current_state->winner = C4_NONE;
    current_state->num_of_pieces = 0;

    states_allocated = 1;

    /* Set up the map */

    map = (int ***) emalloc(size_x * sizeof(int **));
    for (i=0; i<size_x; i++) {
        map[i] = (int **) emalloc(size_y * sizeof(int *));
        for (j=0; j<size_y; j++) {
            map[i][j] = (int *) emalloc((num_to_connect*4 + 1) * sizeof(int));
            map[i][j][0] = -1;
        }
    }

    win_index = 0;

    /* Fill in the horizontal win positions */
    for (i=0; i<size_y; i++)
        for (j=0; j<size_x-num_to_connect+1; j++) {
            for (k=0; k<num_to_connect; k++) {
                win_indices = map[j+k][i];
                for (x=0; win_indices[x] != -1; x++)
                    ;
                win_indices[x++] = win_index;
                win_indices[x] = -1;
            }
            win_index++;
        }

    /* Fill in the vertical win positions */
    for (i=0; i<size_x; i++)
        for (j=0; j<size_y-num_to_connect+1; j++) {
            for (k=0; k<num_to_connect; k++) {
                win_indices = map[i][j+k];
                for (x=0; win_indices[x] != -1; x++)
                    ;
                win_indices[x++] = win_index;
                win_indices[x] = -1;
            }
            win_index++;
        }

    /* Fill in the forward diagonal win positions */
    for (i=0; i<size_y-num_to_connect+1; i++)
        for (j=0; j<size_x-num_to_connect+1; j++) {
            for (k=0; k<num_to_connect; k++) {
                win_indices = map[j+k][i+k];
                for (x=0; win_indices[x] != -1; x++)
                    ;
                win_indices[x++] = win_index;
                win_indices[x] = -1;
            }
            win_index++;
        }

    /* Fill in the backward diagonal win positions */
    for (i=0; i<size_y-num_to_connect+1; i++)
        for (j=size_x-1; j>=num_to_connect-1; j--) {
            for (k=0; k<num_to_connect; k++) {
                win_indices = map[j-k][i+k];
                for (x=0; win_indices[x] != -1; x++)
                    ;
                win_indices[x++] = win_index;
                win_indices[x] = -1;
            }
            win_index++;
        }

    /* Set up the order in which automatic moves should be tried. */
    /* The columns nearer to the center of the board are usually  */
    /* better tactically and are more likely to lead to a win.    */
    /* By ordering the search such that the central columns are   */
    /* tried first, alpha-beta cutoff is much more effective.     */

    drop_order = (int *) emalloc(size_x * sizeof(int));
    column = (size_x-1) / 2;
    for (i=1; i<=size_x; i++) {
        drop_order[i-1] = column;
        column += ((i%2)? i : -i);
    }

    game_in_progress = TRUE;
}


/****************************************************************************/
/**                                                                        **/
/**  This function drops a piece of the specified player into the          **/
/**  specified column.  A value of TRUE is returned if the drop is         **/
/**  successful, or FALSE otherwise.  A drop is unsuccessful if the        **/
/**  specified column number is invalid or full.  If the drop is           **/
/**  successful and row is a non-NULL pointer, the row where the piece     **/
/**  ended up is returned through the row pointer.  Note that column and   **/
/**  row numbering start at 0.                                             **/
/**                                                                        **/
/****************************************************************************/

Boolean
c4_make_move(int player, int column, int *row)
{
    int result; 

    assert(game_in_progress);
    assert(!move_in_progress);

    if (column >= size_x || column < 0)
        return FALSE;

    result = drop_piece(real_player(player), column);
    if (row && result >= 0)
        *row = result;
    return (result >= 0);
}


/****************************************************************************/
/**                                                                        **/
/**  This function instructs the computer to make a move for the specified **/
/**  player.  level specifies the number of levels deep the computer       **/
/**  should search the game tree in order to make its decision.  This      **/
/**  corresponds to the number of "moves" in the game, where each player's **/
/**  turn is considered a move.  A value of TRUE is returned if a move was **/
/**  made, or FALSE otherwise (i.e. if the board is full).  If a move was  **/
/**  made, the column and row where the piece ended up is returned through **/
/**  the column and row pointers (unless a pointer is NULL, in which case  **/
/**  it won't be used to return any information).  Note that column and    **/
/**  row numbering start at 0.  Also note that for a standard 7x6 game of  **/
/**  Connect-4, the computer is brain-dead at levels of three or less,     **/
/**  while at levels of four or more the computer provides a challenge.    **/
/**                                                                        **/
/****************************************************************************/

Boolean
c4_auto_move(int player, int level, int *column, int *row)
{
    int i, best_column = -1, goodness = 0, best_worst = -(INT_MAX);
    int num_of_equal = 0, real_player, current_column, result;

    assert(game_in_progress);
    assert(!move_in_progress);
    assert(level >= 1 && level <= C4_MAX_LEVEL);

    real_player = real_player(player);

    /* It has been proven that the best first move for a standard 7x6 game  */
    /* of connect-4 is the center column.  See Victor Allis' masters thesis */
    /* ("ftp://ftp.cs.vu.nl/pub/victor/connect4.ps") for this proof.        */

    if (current_state->num_of_pieces < 2 &&
                        size_x == 7 && size_y == 6 && num_to_connect == 4 &&
                        (current_state->num_of_pieces == 0 ||
                         current_state->board[3][0] != C4_NONE)) {
        if (column)
            *column = 3;
        if (row)
            *row = current_state->num_of_pieces;
        drop_piece(real_player, 3);
        return TRUE;
    }

    move_in_progress = TRUE;

    /* Simulate a drop in each of the columns and see what the results are. */

    for (i=0; i<size_x; i++) {
        push_state();
        current_column = drop_order[i];

        /* If this column is full, ignore it as a possibility. */
        if (drop_piece(real_player, current_column) < 0) {
            pop_state();
            continue;
        }

        /* If this drop wins the game, take it! */
        else if (current_state->winner == real_player) {
            best_column = current_column;
            pop_state();
            break;
        }

        /* Otherwise, look ahead to see how good this move may turn out */
        /* to be (assuming the opponent makes the best moves possible). */
        else {
            next_poll = clock() + poll_interval;
            goodness = evaluate(real_player, level, -(INT_MAX), -best_worst);
        }

        /* If this move looks better than the ones previously considered, */
        /* remember it.                                                   */
        if (goodness > best_worst) {
            best_worst = goodness;
            best_column = current_column;
            num_of_equal = 1;
        }

        /* If two moves are equally as good, make a random decision. */
        else if (goodness == best_worst) {
            num_of_equal++;
            if (rand()%10000 < ((float)1/(float)num_of_equal) * 10000)
                best_column = current_column;
        }

        pop_state();
    }

    move_in_progress = FALSE;

    /* Drop the piece in the column decided upon. */

    if (best_column >= 0) {
        result = drop_piece(real_player, best_column);
        if (column)
            *column = best_column;
        if (row)
            *row = result;
        return TRUE;
    }
    else
        return FALSE;
}


/****************************************************************************/
/**                                                                        **/
/**  This function returns a two-dimensional array containing the state of **/
/**  the game board.  Do not modify this array.  It is assumed that a game **/
/**  is in progress.  The value of this array is dynamic and will change   **/
/**  to reflect the state of the game as the game progresses.  It becomes  **/
/**  and stays undefined when the game ends.                               **/
/**                                                                        **/
/**  The first dimension specifies the column number and the second        **/
/**  dimension specifies the row number, where column and row numbering    **/
/**  start at 0 and the bottow row is considered the 0th row.  A value of  **/
/**  0 specifies that the position is occupied by a piece owned by player  **/
/**  0, a value of 1 specifies that the position is occupied by a piece    **/
/**  owned by player 1, and a value of C4_NONE specifies that the position **/
/**  is unoccupied.                                                        **/
/**                                                                        **/
/****************************************************************************/

char **
c4_board(void)
{
    assert(game_in_progress);
    return current_state->board;
}


/****************************************************************************/
/**                                                                        **/
/**  This function returns the "score" of the specified player.  This      **/
/**  score is a function of how many winning positions are still available **/
/**  to the player and how close he/she is to achieving each of these      **/
/**  positions.  The scores of both players can be compared to observe how **/
/**  well they are doing relative to each other.                           **/
/**                                                                        **/
/****************************************************************************/

int
c4_score_of_player(int player)
{
    assert(game_in_progress);
    return current_state->score[real_player(player)];
}


/****************************************************************************/
/**                                                                        **/
/**  This function returns TRUE if the specified player has won the game,  **/
/**  and FALSE otherwise.                                                  **/
/**                                                                        **/
/****************************************************************************/

Boolean
c4_is_winner(int player)
{
    assert(game_in_progress);
    return (current_state->winner == real_player(player));
}


/****************************************************************************/
/**                                                                        **/
/**  This function returns TRUE if the board is completely full, and FALSE **/
/**  otherwise.                                                            **/
/**                                                                        **/
/****************************************************************************/

Boolean
c4_is_tie()
{
    assert(game_in_progress);
    return (current_state->num_of_pieces == size_x * size_y);
}


/****************************************************************************/
/**                                                                        **/
/**  This function returns the coordinates of the winning connections of   **/
/**  the winning player.  It is assumed that a player has indeed won the   **/
/**  game.  The coordinates are returned in x1, y1, x2, y2, where (x1, y1) **/
/**  specifies the lower-left piece of the winning connection, and         **/
/**  (x2, y2) specifies the upper-right piece of the winning connection.   **/
/**  If more than one winning connection exists, only one will be          **/
/**  returned.                                                             **/
/**                                                                        **/
/****************************************************************************/

void
c4_win_coords(int *x1, int *y1, int *x2, int *y2)
{
    register int i, j, k;
    int winner, win_pos = 0;
    Boolean found;

    assert(game_in_progress);

    winner = current_state->winner;
    assert(winner != C4_NONE);

    while (current_state->score_array[winner][win_pos] != magic_win_number)
        win_pos++;

    /* Find the lower-left piece of the winning connection. */

    found = FALSE;
    for (j=0; j<size_y && !found; j++)
        for (i=0; i<size_x; i++)
            for (k=0; map[i][j][k] != -1; k++)
                if (map[i][j][k] == win_pos) {
                    *x1 = i;
                    *y1 = j;
                    found = TRUE;
                    break;
                }

    /* Find the upper-right piece of the winning connection. */

    found = FALSE;
    for (j=size_y-1; j>=0 && !found; j--)
        for (i=size_x-1; i>=0; i--)
            for (k=0; map[i][j][k] != -1; k++)
                if (map[i][j][k] == win_pos) {
                    *x2 = i;
                    *y2 = j;
                    found = TRUE;
                    break;
                }
}


/****************************************************************************/
/**                                                                        **/
/**  This function ends the current game.  It is assumed that a game is    **/
/**  in progress.  It is illegal to call any other game function           **/
/**  immediately after this one except for c4_new_game(), c4_poll() and    **/
/**  c4_reset().                                                           **/
/**                                                                        **/
/****************************************************************************/

void
c4_end_game(void)
{
    int i, j;

    assert(game_in_progress);
    assert(!move_in_progress);

    /* Free up the memory used by the map. */

    for (i=0; i<size_x; i++) {
        for (j=0; j<size_y; j++)
            free(map[i][j]);
        free(map[i]);
    }
    free(map);

    /* Free up the memory of all the states used. */

    for (i=0; i<states_allocated; i++) {
        for (j=0; j<size_x; j++)
            free(state_stack[i].board[j]);
        free(state_stack[i].board);
        free(state_stack[i].score_array[0]);
        free(state_stack[i].score_array[1]);
    }
    states_allocated = 0;

    /* Free up the memory used by the drop_order array. */

    free(drop_order);

    game_in_progress = FALSE;
}


/****************************************************************************/
/**                                                                        **/
/**  This function resets the state of the algorithm to the starting state **/
/**  (i.e., no game in progress and a NULL poll function).  There should   **/
/**  no reason to call this function unless for some reason the calling    **/
/**  algorithm loses track of the game state.  It is illegal to call any   **/
/**  other game function immediately after this one except for             **/
/**  c4_new_game(), c4_poll() and c4_reset().                              **/
/**                                                                        **/
/****************************************************************************/

void
c4_reset(void)
{
    assert(!move_in_progress);
    if (game_in_progress)
        c4_end_game();
    poll_function = NULL;
}

/****************************************************************************/
/**                                                                        **/
/**  This function returns the RCS string representing the version of      **/
/**  this Connect-4 implementation.                                        **/
/**                                                                        **/
/****************************************************************************/

const char *
c4_get_version(void)
{
    return "$Id: c4.c,v 3.7 2000/05/19 16:49:46 pomakis Exp pomakis $";
}


/****************************************************************************/
/****************************************************************************/
/**                                                                        **/
/**  The following functions are local to this file and should not be      **/
/**  called externally.                                                    **/
/**                                                                        **/
/****************************************************************************/
/****************************************************************************/


/****************************************************************************/
/**                                                                        **/
/**  This function returns the number of possible win positions on a board **/
/**  of dimensions x by y with n being the number of pieces required in a  **/
/**  row in order to win.                                                  **/
/**                                                                        **/
/****************************************************************************/

static int
num_of_win_places(int x, int y, int n)
{
    if (x < n && y < n)
        return 0;
    else if (x < n)
        return x * ((y-n)+1);
    else if (y < n)
        return y * ((x-n)+1);
    else
        return 4*x*y - 3*x*n - 3*y*n + 3*x + 3*y - 4*n + 2*n*n + 2;
}


/****************************************************************************/
/**                                                                        **/
/**  This function updates the score of the specified player in the        **/
/**  context of the current state,  given that the player has just placed  **/
/**  a game piece in column x, row y.                                      **/
/**                                                                        **/
/****************************************************************************/

static void
update_score(int player, int x, int y)
{
    register int i;
    int win_index;
    int this_difference = 0, other_difference = 0;
    int **current_score_array = current_state->score_array;
    int other_player = other(player);

    for (i=0; map[x][y][i] != -1; i++) {
        win_index = map[x][y][i];
        this_difference += current_score_array[player][win_index];
        other_difference += current_score_array[other_player][win_index];

        current_score_array[player][win_index] <<= 1;
        current_score_array[other_player][win_index] = 0;

        if (current_score_array[player][win_index] == magic_win_number)
            if (current_state->winner == C4_NONE)
                current_state->winner = player;
    }

    current_state->score[player] += this_difference;
    current_state->score[other_player] -= other_difference;
}


/****************************************************************************/
/**                                                                        **/
/**  This function drops a piece of the specified player into the          **/
/**  specified column.  The row where the piece ended up is returned, or   **/
/**  -1 if the drop was unsuccessful (i.e., the specified column is full). **/
/**                                                                        **/
/****************************************************************************/

static int
drop_piece(int player, int column)
{
    int y = 0;

    while (current_state->board[column][y] != C4_NONE && ++y < size_y)
        ;

    if (y == size_y)
        return -1;

    current_state->board[column][y] = player;
    current_state->num_of_pieces++;
    update_score(player, column, y);

    return y;
}


/****************************************************************************/
/**                                                                        **/
/**  This function pushes the current state onto a stack.  popdir() is     **/
/**  used to pop from this stack.                                          **/
/**                                                                        **/
/**  Technically what it does, since the current state is considered to    **/
/**  be the top of the stack, is push a copy of the current state onto     **/
/**  the stack right above it.  The stack pointer (depth) is then          **/
/**  incremented so that the new copy is considered to be the current      **/
/**  state.  That way, all pop_state() has to do is decrement the stack    **/
/**  pointer.                                                              **/
/**                                                                        **/
/**  For efficiency, memory for each stack state used is only allocated    **/
/**  once per game, and reused for the remainder of the game.              **/
/**                                                                        **/
/****************************************************************************/

static void
push_state(void)
{
    register int i, win_places_array_size;
    Game_state *old_state, *new_state;

    win_places_array_size = win_places * sizeof(int);
    old_state = &state_stack[depth++];
    new_state = &state_stack[depth];

    if (depth == states_allocated) {

        /* Allocate space for the board */

        new_state->board = (char **) emalloc(size_x * sizeof(char *));
        for (i=0; i<size_x; i++)
            new_state->board[i] = (char *) emalloc(size_y);

        /* Allocate space for the score array */

        new_state->score_array[0] = (int *) emalloc(win_places_array_size);
        new_state->score_array[1] = (int *) emalloc(win_places_array_size);

        states_allocated++;
    }

    /* Copy the board */

    for (i=0; i<size_x; i++)
        memcpy(new_state->board[i], old_state->board[i], size_y);

    /* Copy the score array */

    memcpy(new_state->score_array[0], old_state->score_array[0],
           win_places_array_size);
    memcpy(new_state->score_array[1], old_state->score_array[1],
           win_places_array_size);

    new_state->score[0] = old_state->score[0];
    new_state->score[1] = old_state->score[1];
    new_state->winner = old_state->winner;

    current_state = new_state;
}


/****************************************************************************/
/**                                                                        **/
/**  This recursive function determines how good the current state may     **/
/**  turn out to be for the specified player.  It does this by looking     **/
/**  ahead level moves.  It is assumed that both the specified player and  **/
/**  the opponent may make the best move possible.  alpha and beta are     **/
/**  used for alpha-beta cutoff so that the game tree can be pruned to     **/
/**  avoid searching unneccessary paths.                                   **/
/**                                                                        **/
/**  The specified poll function (if any) is called at the appropriate     **/
/**  intervals.                                                            **/
/**                                                                        **/
/**  The worst goodness that the current state can produce in the number   **/
/**  of moves (levels) searched is returned.  This is the best the         **/
/**  specified player can hope to achieve with this state (since it is     **/
/**  assumed that the opponent will make the best moves possible).         **/
/**                                                                        **/
/****************************************************************************/

static int
evaluate(int player, int level, int alpha, int beta)
{
    int i, goodness, best, maxab;

    if (poll_function && next_poll <= clock()) {
        next_poll += poll_interval;
        (*poll_function)();
    }

    if (level == depth)
        return goodness_of(player);
    else {
        /* Assume it is the other player's turn. */
        best = -(INT_MAX);
        maxab = alpha;
        for(i=0; i<size_x; i++) {
            push_state();
            if (drop_piece(other(player), drop_order[i]) < 0) {
                pop_state();
                continue;
            }
            else if (current_state->winner == other(player))
                goodness = INT_MAX - depth;
            else
                goodness = evaluate(other(player), level, -beta, -maxab);
            if (goodness > best) {
                best = goodness;
                if (best > maxab)
                    maxab = best;
            }
            pop_state();
            if (best > beta)
                break;
        }

        /* What's good for the other player is bad for this one. */
        return -best;
    }
}


/****************************************************************************/
/**                                                                        **/
/**  A safer version of malloc().                                          **/
/**                                                                        **/
/****************************************************************************/

static void *
emalloc(unsigned int n)
{
    void *ptr;

    ptr = (void *) malloc(n);
    if (ptr == NULL) {
        fprintf(stderr, "c4: emalloc() - Can't allocate %d bytes.\n", n);
        exit(1);
    }
    return ptr;
}
