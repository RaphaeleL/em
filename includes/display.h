#ifndef DISPLAY_H
#define DISPLAY_H

#include <ncurses.h>
#include "buffer.h"

typedef struct {
    Buffer *buf;
    int cx, cy;      // cursor (col, row) in buffer coords
    int goal_cx;     // preferred column for vertical movement
    int row_offset;  // top line index for viewport
    int col_offset;  // left col index for horizontal scroll
    int screen_rows, screen_cols;
    WINDOW *win;
    char minibuf[512];

    // mark / region (selection)
    int mark_x, mark_y;
    int mark_active;

    // kill ring (single slot)
    char *kill_buf;
    int last_was_kill;   // consecutive C-k kills append
} EditorState;

void editor_update_screen_size(EditorState *E);
void editor_clamp_cursor(EditorState *E);
void editor_scroll_to_cursor(EditorState *E);
void editor_draw(EditorState *E, const char *message);
void editor_message(EditorState *E, const char *fmt, ...);
void editor_move_cursor_left(EditorState *E);
void editor_move_cursor_right(EditorState *E);
void editor_move_cursor_up(EditorState *E);
void editor_move_cursor_down(EditorState *E);
void editor_move_cursor_to_beginning_of_line(EditorState *E);
void editor_move_cursor_to_end_of_line(EditorState *E);
void editor_move_cursor_forward_word(EditorState *E);
void editor_move_cursor_backward_word(EditorState *E);
void editor_move_to_buffer_start(EditorState *E);
void editor_move_to_buffer_end(EditorState *E);
void editor_scroll_page_down(EditorState *E);
void editor_scroll_page_up(EditorState *E);
void editor_recenter(EditorState *E);

// region helpers: normalized bounds, 0 if no active region
int editor_region_bounds(EditorState *E, int *sy, int *sx, int *ey, int *ex);

#endif // DISPLAY_H
