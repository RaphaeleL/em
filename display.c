/* 
 * display.c
 *
 * Minimal display and cursor management for a simple text editor.
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

#include "includes/display.h"

void editor_update_screen_size(EditorState *E) {
    getmaxyx(stdscr, E->screen_rows, E->screen_cols);
}

void editor_draw(EditorState *E, const char *message) {
    werase(stdscr);
    editor_update_screen_size(E);
    int rows = E->screen_rows - 2; // keep last 2 lines for status & minibuffer
    int cols = E->screen_cols;

    // draw buffer lines
    for (int i = 0; i < rows; ++i) {
        int lineno = E->row_offset + i;
        if (lineno >= E->buf->nlines) break;
        const char *ln = E->buf->lines[lineno];
        // handle horizontal scroll naive
        int start = E->col_offset;
        int len = strlen(ln);
        if (start < len) {
            mvaddnstr(i, 0, ln + start, cols);
        }
    }

    // status line
    attron(A_REVERSE);
    char status[512];
    snprintf(status, sizeof(status), " %s %s | %d/%d ",
             E->buf->filename ? E->buf->filename : "[NoName]",
             E->buf->modified ? "(modified)" : "",
             E->cy + 1, E->buf->nlines);
    mvaddnstr(rows, 0, status, cols);
    // rest of pad
    for (int i = strlen(status); i < cols; ++i) mvaddch(rows, i, ' ');
    attroff(A_REVERSE);

    // minibuffer line
    mvaddnstr(rows + 1, 0, E->minibuf, cols);

    // move cursor
    int curs_y = E->cy - E->row_offset;
    int curs_x = 0;
    if (E->cy < E->buf->nlines) {
        const char *line = E->buf->lines[E->cy];
        int llen = strlen(line);
        int col = E->cx;
        if (col > llen) col = llen;
        curs_x = col - E->col_offset;
        if (curs_y >= 0 && curs_y < rows && curs_x >= 0 && curs_x < cols)
            move(curs_y, curs_x);
        else
            move(rows, 0); // put somewhere safe
    } else {
        move(rows, 0);
    }

    refresh();

    // show a transient message in status if provided
    if (message && *message) {
        int mline = rows + 1;
        int max = cols;
        mvaddnstr(mline, 0, message, max);
        refresh();
    }
}

void editor_message(EditorState *E, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(E->minibuf, sizeof(E->minibuf), fmt, ap);
    va_end(ap);
    editor_draw(E, NULL);
}

void editor_move_cursor_left(EditorState *E) {
    if (E->cx > 0) {
        E->cx--;
    } else if (E->cy > 0) {
        E->cy--;
        E->cx = strlen(E->buf->lines[E->cy]);
        if (E->cy < E->row_offset) E->row_offset = E->cy;
    }
}

void editor_move_cursor_right(EditorState *E) {
    int llen = strlen(E->buf->lines[E->cy]);
    if (E->cx < llen) {
        E->cx++;
    } else if (E->cy + 1 < E->buf->nlines) {
        E->cy++;
        E->cx = 0;
        int rows = E->screen_rows - 2;
        if (E->cy >= E->row_offset + rows) E->row_offset = E->cy - rows + 1;
    }
}

void editor_move_cursor_up(EditorState *E) {
    if (E->cy > 0) {
        E->cy--;
        int llen = strlen(E->buf->lines[E->cy]);
        if (E->cx > llen) E->cx = llen;
        if (E->cy < E->row_offset) E->row_offset = E->cy;
    }
}

void editor_move_cursor_down(EditorState *E) {
    if (E->cy + 1 < E->buf->nlines) {
        E->cy++;
        int llen = strlen(E->buf->lines[E->cy]);
        if (E->cx > llen) E->cx = llen;
        int rows = E->screen_rows - 2;
        if (E->cy >= E->row_offset + rows) E->row_offset = E->cy - rows + 1;
    }
}

