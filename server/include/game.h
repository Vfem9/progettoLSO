#ifndef GAME_H
#define GAME_H

#define BOARD_ROWS 6
#define BOARD_COLS 7
#define WINNING_LENGTH 4

typedef int Board[BOARD_ROWS][BOARD_COLS];

int validate_move(Board b, int column);
int apply_move(Board b, int column, int player);
int check_win(Board b, int row, int col, int player);
int check_draw(Board b);
void init_board(Board b);

#endif