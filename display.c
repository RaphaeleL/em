/* 
 * display.c
 *
 * Display, cursor movement and region handling for the editor.
 *
 * Created at:  12. Sep 2025 
 * Author:      Raphaele Salvatore Licciardo 
 *
 *
 * Copyright (c) 2025 Raphaele Salvatore Licciardo
 *
 */

#include <ncurses.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

#include "includes/display.h"

void editor_update_screen_size(EditorState *E) {
    getmaxyx(stdscr, E->screen_rows, E->screen_cols);
    if (E->screen_rows < 3) E->screen_rows = 3;
    if (E->screen_cols < 1) E->screen_cols = 1;
}

static int text_rows(EditorState *E) {
    int rows = E->screen_rows - 2; // keep last 2 lines for status & minibuffer
    return rows > 0 ? rows : 1;
}

// Keep cursor (and mark) inside the buffer at all times. Every entry point
// into drawing or editing goes through this, so no stale coordinate can
// ever index out of bounds.
void editor_clamp_cursor(EditorState *E) {
    Buffer *b = E->buf;
    if (E->cy < 0) E->cy = 0;
    if (E->cy >= b->nlines) E->cy = b->nlines - 1;
    int llen = (int)strlen(b->lines[E->cy]);
    if (E->cx < 0) E->cx = 0;
    if (E->cx > llen) E->cx = llen;

    if (E->mark_active) {
        if (E->mark_y < 0) E->mark_y = 0;
        if (E->mark_y >= b->nlines) E->mark_y = b->nlines - 1;
        int mlen = (int)strlen(b->lines[E->mark_y]);
        if (E->mark_x < 0) E->mark_x = 0;
        if (E->mark_x > mlen) E->mark_x = mlen;
    }
}

void editor_scroll_to_cursor(EditorState *E) {
    int rows = text_rows(E);
    int cols = E->screen_cols;

    if (E->cy < E->row_offset) E->row_offset = E->cy;
    if (E->cy >= E->row_offset + rows) E->row_offset = E->cy - rows + 1;
    if (E->row_offset < 0) E->row_offset = 0;

    if (E->cx < E->col_offset) E->col_offset = E->cx;
    if (E->cx >= E->col_offset + cols) E->col_offset = E->cx - cols + 1;
    if (E->col_offset < 0) E->col_offset = 0;
}

// Normalized region bounds (start before end). Returns 0 if no active region.
int editor_region_bounds(EditorState *E, int *sy, int *sx, int *ey, int *ex) {
    if (!E->mark_active) return 0;
    if (E->mark_y < E->cy || (E->mark_y == E->cy && E->mark_x <= E->cx)) {
        *sy = E->mark_y; *sx = E->mark_x;
        *ey = E->cy;     *ex = E->cx;
    } else {
        *sy = E->cy;     *sx = E->cx;
        *ey = E->mark_y; *ex = E->mark_x;
    }
    if (*sy == *ey && *sx == *ex) return 0;
    return 1;
}

void editor_draw(EditorState *E, const char *message) {
    editor_update_screen_size(E);
    editor_clamp_cursor(E);
    editor_scroll_to_cursor(E);

    erase();
    int rows = text_rows(E);
    int cols = E->screen_cols;

    int sy = 0, sx = 0, ey = 0, ex = 0;
    int has_region = editor_region_bounds(E, &sy, &sx, &ey, &ex);

    // draw buffer lines
    for (int i = 0; i < rows; ++i) {
        int lineno = E->row_offset + i;
        if (lineno >= E->buf->nlines) break;
        const char *ln = E->buf->lines[lineno];
        int len = (int)strlen(ln);

        // selection span on this line, in buffer columns
        int hs = -1, he = -1;
        if (has_region && lineno >= sy && lineno <= ey) {
            hs = (lineno == sy) ? sx : 0;
            he = (lineno == ey) ? ex : len;
        }

        for (int col = E->col_offset; col < len && col - E->col_offset < cols; ++col) {
            int highlighted = (hs >= 0 && col >= hs && col < he);
            if (highlighted) attron(A_REVERSE);
            mvaddch(i, col - E->col_offset, (chtype)(unsigned char)ln[col]);
            if (highlighted) attroff(A_REVERSE);
        }
        // show selection of the line's newline (region continues past EOL)
        if (hs >= 0 && he >= len && lineno < ey && len - E->col_offset >= 0
            && len - E->col_offset < cols) {
            attron(A_REVERSE);
            mvaddch(i, len - E->col_offset, ' ');
            attroff(A_REVERSE);
        }
    }

    // status line
    attron(A_REVERSE);
    char status[512];
    const char *readonly_str = buffer_is_readonly(E->buf) ? " RO" : "";
    snprintf(status, sizeof(status), " %s%s%s%s  L%d/%d C%d",
             E->buf->filename ? E->buf->filename : "[NoName]",
             E->buf->modified ? " *" : "",
             readonly_str,
             E->mark_active ? "  [mark]" : "",
             E->cy + 1, E->buf->nlines, E->cx + 1);
    mvaddnstr(rows, 0, status, cols);
    for (int i = (int)strlen(status); i < cols; ++i) mvaddch(rows, i, ' ');
    attroff(A_REVERSE);

    // minibuffer line
    mvaddnstr(rows + 1, 0, message && *message ? message : E->minibuf, cols);

    // move cursor
    int curs_y = E->cy - E->row_offset;
    int curs_x = E->cx - E->col_offset;
    if (curs_y >= 0 && curs_y < rows && curs_x >= 0 && curs_x < cols)
        move(curs_y, curs_x);
    else
        move(rows, 0);

    refresh();
}

void editor_message(EditorState *E, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(E->minibuf, sizeof(E->minibuf), fmt, ap);
    va_end(ap);
    editor_draw(E, NULL);
}

void editor_move_cursor_left(EditorState *E) {
    editor_clamp_cursor(E);
    if (E->cx > 0) {
        E->cx--;
    } else if (E->cy > 0) {
        E->cy--;
        E->cx = (int)strlen(E->buf->lines[E->cy]);
    }
    E->goal_cx = E->cx;
}

void editor_move_cursor_right(EditorState *E) {
    editor_clamp_cursor(E);
    int llen = (int)strlen(E->buf->lines[E->cy]);
    if (E->cx < llen) {
        E->cx++;
    } else if (E->cy + 1 < E->buf->nlines) {
        E->cy++;
        E->cx = 0;
    }
    E->goal_cx = E->cx;
}

void editor_move_cursor_up(EditorState *E) {
    editor_clamp_cursor(E);
    if (E->cy > 0) {
        E->cy--;
        int llen = (int)strlen(E->buf->lines[E->cy]);
        E->cx = E->goal_cx < llen ? E->goal_cx : llen;
    }
}

void editor_move_cursor_down(EditorState *E) {
    editor_clamp_cursor(E);
    if (E->cy + 1 < E->buf->nlines) {
        E->cy++;
        int llen = (int)strlen(E->buf->lines[E->cy]);
        E->cx = E->goal_cx < llen ? E->goal_cx : llen;
    }
}

void editor_move_cursor_to_beginning_of_line(EditorState *E) {
    E->cx = 0;
    E->goal_cx = 0;
}

void editor_move_cursor_to_end_of_line(EditorState *E) {
    editor_clamp_cursor(E);
    E->cx = (int)strlen(E->buf->lines[E->cy]);
    E->goal_cx = E->cx;
}

void editor_move_to_buffer_start(EditorState *E) {
    E->cy = 0;
    E->cx = 0;
    E->goal_cx = 0;
}

void editor_move_to_buffer_end(EditorState *E) {
    E->cy = E->buf->nlines - 1;
    E->cx = (int)strlen(E->buf->lines[E->cy]);
    E->goal_cx = E->cx;
}

// Helper function to check if a character is a word character
static int is_word_char(char c) {
    return isalnum((unsigned char)c) || c == '_';
}

void editor_move_cursor_forward_word(EditorState *E) {
    editor_clamp_cursor(E);
    Buffer *b = E->buf;
    // skip non-word characters (crossing line boundaries)
    while (1) {
        const char *line = b->lines[E->cy];
        int len = (int)strlen(line);
        if (E->cx >= len) {
            if (E->cy + 1 >= b->nlines) break;
            E->cy++;
            E->cx = 0;
            continue;
        }
        if (is_word_char(line[E->cx])) break;
        E->cx++;
    }
    // skip the word itself
    {
        const char *line = b->lines[E->cy];
        int len = (int)strlen(line);
        while (E->cx < len && is_word_char(line[E->cx])) E->cx++;
    }
    E->goal_cx = E->cx;
}

void editor_move_cursor_backward_word(EditorState *E) {
    editor_clamp_cursor(E);
    Buffer *b = E->buf;
    // skip non-word characters backwards (crossing line boundaries)
    while (1) {
        if (E->cx == 0) {
            if (E->cy == 0) break;
            E->cy--;
            E->cx = (int)strlen(b->lines[E->cy]);
            continue;
        }
        if (is_word_char(b->lines[E->cy][E->cx - 1])) break;
        E->cx--;
    }
    // skip the word itself
    while (E->cx > 0 && is_word_char(b->lines[E->cy][E->cx - 1])) E->cx--;
    E->goal_cx = E->cx;
}

void editor_scroll_page_down(EditorState *E) {
    int rows = text_rows(E);
    int max_offset = E->buf->nlines - rows;
    if (max_offset < 0) max_offset = 0;

    E->row_offset += rows;
    if (E->row_offset > max_offset) E->row_offset = max_offset;

    E->cy += rows;
    editor_clamp_cursor(E);
    int llen = (int)strlen(E->buf->lines[E->cy]);
    E->cx = E->goal_cx < llen ? E->goal_cx : llen;
}

void editor_scroll_page_up(EditorState *E) {
    int rows = text_rows(E);

    E->row_offset -= rows;
    if (E->row_offset < 0) E->row_offset = 0;

    E->cy -= rows;
    editor_clamp_cursor(E);
    int llen = (int)strlen(E->buf->lines[E->cy]);
    E->cx = E->goal_cx < llen ? E->goal_cx : llen;
}

void editor_recenter(EditorState *E) {
    int rows = text_rows(E);
    E->row_offset = E->cy - rows / 2;
    if (E->row_offset < 0) E->row_offset = 0;
}
