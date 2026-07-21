#include <string.h>
#include "../include/game.h"

void init_board(Board b) {
    for (int row = 0; row < BOARD_ROWS; row++) {
        for (int col = 0; col < BOARD_COLS; col++) {
            b[row][col] = 0;
        }
    }
}

int validate_move(Board b, int column) {
    if (column < 0 || column >= BOARD_COLS) {
        return 0;
    }

    if (b[0][column] != 0) {
        return 0;
    }

    return 1;
}

int apply_move(Board b, int column, int player) {
    if (!validate_move(b, column)) {
        return -1;
    }

    for (int row = BOARD_ROWS - 1; row >= 0; row--) {
        if (b[row][column] == 0) {
            b[row][column] = player;
            return row;
        }
    }

    return -1;
}

static int count_consecutive(Board b, int row, int col, int player, int dr, int dc) {
    int count = 0;
    int r = row + dr;
    int c = col + dc;

    while (r >= 0 && r < BOARD_ROWS && c >= 0 && c < BOARD_COLS && b[r][c] == player) {
        count++;
        r += dr;
        c += dc;
    }

    return count;
}

int check_win(Board b, int row, int col, int player) {
    int h_left = count_consecutive(b, row, col, player, 0, -1);
    int h_right = count_consecutive(b, row, col, player, 0, 1);
    if (h_left + h_right + 1 >= WINNING_LENGTH) {
        return 1;
    }

    int v_down = count_consecutive(b, row, col, player, 1, 0);
    if (v_down + 1 >= WINNING_LENGTH) {
        return 1;
    }

    int d1_up_left = count_consecutive(b, row, col, player, -1, -1);
    int d1_down_right = count_consecutive(b, row, col, player, 1, 1);
    if (d1_up_left + d1_down_right + 1 >= WINNING_LENGTH) {
        return 1;
    }

    int d2_up_right = count_consecutive(b, row, col, player, -1, 1);
    int d2_down_left = count_consecutive(b, row, col, player, 1, -1);
    if (d2_up_right + d2_down_left + 1 >= WINNING_LENGTH) {
        return 1;
    }

    return 0;
}

int check_draw(Board b) {
    for (int col = 0; col < BOARD_COLS; col++) {
        if (b[0][col] == 0) {
            return 0;
        }
    }
    return 1;
}
