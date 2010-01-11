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
**                  See the file "c4.c" for documentation.                **
**                                                                        **
****************************************************************************
**  $Id: c4.h,v 3.7 2000/05/19 16:49:46 pomakis Exp pomakis $
***************************************************************************/

#ifndef C4_DEFINED
#define C4_DEFINED

#include <time.h>

#ifndef Boolean
#define Boolean char
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#define C4_NONE      2
#define C4_MAX_LEVEL 20

/* See the file "c4.c" for documentation on the following functions. */

extern void    c4_poll(void (*poll_func)(void), clock_t interval);
extern void    c4_new_game(int width, int height, int num);
extern Boolean c4_make_move(int player, int column, int *row);
extern Boolean c4_auto_move(int player, int level, int *column, int *row);
extern char ** c4_board(void);
extern int     c4_score_of_player(int player);
extern Boolean c4_is_winner(int player);
extern Boolean c4_is_tie(void);
extern void    c4_win_coords(int *x1, int *y1, int *x2, int *y2);
extern void    c4_end_game(void);
extern void    c4_reset(void);

extern const char *c4_get_version(void);

#endif /* C4_DEFINED */
