#ifndef DISPLAY_H
#define DISPLAY_H

#include <ncurses.h>
#include "buffer.h"

typedef struct {
    Buffer *buf;
    int cx, cy;      // cursor (col, row) in buffer coords
    int row_offset;  // top line index for viewport
    int col_offset;  // left col index for horizontal scroll (not used much)
    int screen_rows, screen_cols;
    WINDOW *win;
    char minibuf[512];
    int minibuf_len;
    int last_message_time;
} EditorState;

void editor_update_screen_size(EditorState *E); 
void editor_draw(EditorState *E, const char *message);
void editor_message(EditorState *E, const char *fmt, ...);
void editor_move_cursor_left(EditorState *E);
void editor_move_cursor_right(EditorState *E);
void editor_move_cursor_up(EditorState *E);
void editor_move_cursor_down(EditorState *E);

#endif // DISPLAY_H
