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
    const char *readonly_str = buffer_is_readonly(E->buf) ? " (read-only)" : "";
    snprintf(status, sizeof(status), " %s %s%s | %d/%d ",
             E->buf->filename ? E->buf->filename : "[NoName]",
             E->buf->modified ? "(modified)" : "",
             readonly_str,
             E->cy + 1, E->buf->nlines);
    mvaddnstr(rows, 0, status, cols);
    // rest of pad
    for (int i = strlen(status); i < cols; ++i) mvaddch(rows, i, ' ');
    attroff(A_REVERSE);

    // minibuffer line
    mvaddnstr(rows + 1, 0, E->minibuf, cols);

    // draw completion popup if visible
    if (E->popup_visible) {
        editor_draw_completion_popup(E);
    }

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

void editor_move_cursor_to_beginning_of_line(EditorState *E) {
    E->cx = 0;
    // Adjust horizontal scroll if needed
    if (E->col_offset > 0) {
        E->col_offset = 0;
    }
}

void editor_move_cursor_to_end_of_line(EditorState *E) {
    if (E->cy < E->buf->nlines) {
        E->cx = strlen(E->buf->lines[E->cy]);
    }
}

// Helper function to check if a character is a word character
static int is_word_char(char c) {
    return isalnum(c) || c == '_';
}

void editor_move_cursor_forward_word(EditorState *E) {
    if (E->cy >= E->buf->nlines) return;
    
    const char *line = E->buf->lines[E->cy];
    int line_len = strlen(line);
    
    // If we're at the end of the line, move to the beginning of the next line
    if (E->cx >= line_len) {
        if (E->cy + 1 < E->buf->nlines) {
            E->cy++;
            E->cx = 0;
            int rows = E->screen_rows - 2;
            if (E->cy >= E->row_offset + rows) E->row_offset = E->cy - rows + 1;
        }
        return;
    }
    
    // Skip current word if we're in the middle of one
    while (E->cx < line_len && is_word_char(line[E->cx])) {
        E->cx++;
    }
    
    // Skip whitespace
    while (E->cx < line_len && !is_word_char(line[E->cx])) {
        E->cx++;
    }
    
    // If we're still at the end of the line, move to next line
    if (E->cx >= line_len) {
        if (E->cy + 1 < E->buf->nlines) {
            E->cy++;
            E->cx = 0;
            int rows = E->screen_rows - 2;
            if (E->cy >= E->row_offset + rows) E->row_offset = E->cy - rows + 1;
        }
    }
}

void editor_move_cursor_backward_word(EditorState *E) {
    // If we're at the beginning of the line, move to the end of the previous line
    if (E->cx == 0) {
        if (E->cy > 0) {
            E->cy--;
            E->cx = strlen(E->buf->lines[E->cy]);
            if (E->cy < E->row_offset) E->row_offset = E->cy;
        }
        return;
    }
    
    if (E->cy >= E->buf->nlines) return;
    
    const char *line = E->buf->lines[E->cy];
    
    // Move back one character first
    E->cx--;
    
    // Skip whitespace
    while (E->cx > 0 && !is_word_char(line[E->cx])) {
        E->cx--;
    }
    
    // Skip current word
    while (E->cx > 0 && is_word_char(line[E->cx - 1])) {
        E->cx--;
    }
}

void editor_scroll_page_down(EditorState *E) {
    int rows = E->screen_rows - 2; // exclude status and minibuffer
    int max_offset = E->buf->nlines - rows;
    
    if (max_offset < 0) max_offset = 0;
    
    // Scroll down by one page
    E->row_offset += rows;
    if (E->row_offset > max_offset) {
        E->row_offset = max_offset;
    }
    
    // Move cursor to maintain relative position, but don't go beyond buffer
    E->cy += rows;
    if (E->cy >= E->buf->nlines) {
        E->cy = E->buf->nlines - 1;
    }
    
    // Adjust cursor position if it's beyond the line length
    if (E->cy >= 0 && E->cy < E->buf->nlines) {
        int line_len = strlen(E->buf->lines[E->cy]);
        if (E->cx > line_len) {
            E->cx = line_len;
        }
    }
}

void editor_scroll_page_up(EditorState *E) {
    int rows = E->screen_rows - 2; // exclude status and minibuffer
    
    // Scroll up by one page
    E->row_offset -= rows;
    if (E->row_offset < 0) {
        E->row_offset = 0;
    }
    
    // Move cursor to maintain relative position
    E->cy -= rows;
    if (E->cy < 0) {
        E->cy = 0;
    }
    
    // Adjust cursor position if it's beyond the line length
    if (E->cy >= 0 && E->cy < E->buf->nlines) {
        int line_len = strlen(E->buf->lines[E->cy]);
        if (E->cx > line_len) {
            E->cx = line_len;
        }
    }
}

// Completion popup implementation
void editor_show_completion_popup(EditorState *E, FileCompletion *fc, int x, int y) {
    (void)x; // x parameter not used for full-width popup
    E->completion = fc;
    E->popup_visible = 1;
    E->popup_x = 0; // Always start at left edge for full width
    E->popup_y = y;
    
    // Use full screen width like in real Emacs
    E->popup_width = E->screen_cols;
    E->popup_height = fc->count + 2; // +2 for borders
    
    // Limit height to screen (leave space for minibuffer)
    if (E->popup_height > E->screen_rows - 2) {
        E->popup_height = E->screen_rows - 2;
    }
    
    // Position popup below minibuffer
    if (E->popup_y + E->popup_height > E->screen_rows - 1) {
        E->popup_y = E->screen_rows - E->popup_height - 1;
    }
    
    // Initialize scroll offset to ensure selected item is visible
    E->popup_scroll_offset = 0;
    int visible_items = E->popup_height - 2;
    if (fc->selected >= visible_items) {
        E->popup_scroll_offset = fc->selected - visible_items + 1;
    }
}

void editor_hide_completion_popup(EditorState *E) {
    E->popup_visible = 0;
    E->completion = NULL;
}

void editor_draw_completion_popup(EditorState *E) {
    if (!E->popup_visible || !E->completion) return;
    
    // Ensure scroll offset is valid
    int visible_items = E->popup_height - 2; // exclude borders
    if (E->popup_scroll_offset < 0) {
        E->popup_scroll_offset = 0;
    }
    if (E->popup_scroll_offset > E->completion->count - visible_items) {
        E->popup_scroll_offset = E->completion->count - visible_items;
        if (E->popup_scroll_offset < 0) E->popup_scroll_offset = 0;
    }
    
    // Ensure selected item is visible
    if (E->completion->selected < E->popup_scroll_offset) {
        E->popup_scroll_offset = E->completion->selected;
    } else if (E->completion->selected >= E->popup_scroll_offset + visible_items) {
        E->popup_scroll_offset = E->completion->selected - visible_items + 1;
    }
    
    // Save current attributes
    int old_attr = getattrs(stdscr);
    
    // Draw popup border (full width)
    attron(A_REVERSE);
    for (int y = 0; y < E->popup_height; ++y) {
        for (int x = 0; x < E->popup_width; ++x) {
            if (y == 0 || y == E->popup_height - 1 || x == 0 || x == E->popup_width - 1) {
                mvaddch(E->popup_y + y, E->popup_x + x, ' ');
            }
        }
    }
    attroff(A_REVERSE);
    
    // Draw completion items
    int start_idx = E->popup_scroll_offset;
    int end_idx = start_idx + visible_items;
    if (end_idx > E->completion->count) {
        end_idx = E->completion->count;
    }
    
    for (int i = start_idx; i < end_idx; ++i) {
        int y = E->popup_y + 1 + (i - start_idx);
        int x = E->popup_x + 1;
        
        // Highlight selected item
        if (i == E->completion->selected) {
            attron(A_REVERSE);
        }
        
        // Use full width for display
        const char *filename = E->completion->matches[i];
        int max_len = E->popup_width - 2;
        
        // Create display string with padding to fill width
        char display[512];
        strncpy(display, filename, sizeof(display) - 1);
        display[sizeof(display) - 1] = '\0';
        
        // Pad with spaces to fill the width
        int len = strlen(display);
        if (len < max_len) {
            memset(display + len, ' ', max_len - len);
            display[max_len] = '\0';
        } else if (len > max_len) {
            // Truncate if too long
            display[max_len - 3] = '\0';
            strcat(display, "...");
        }
        
        mvaddstr(y, x, display);
        
        if (i == E->completion->selected) {
            attroff(A_REVERSE);
        }
    }
    
    // Restore attributes
    attron(old_attr);
}

void editor_completion_scroll_up(EditorState *E) {
    if (!E->popup_visible || !E->completion) return;
    
    if (E->completion->selected > 0) {
        E->completion->selected--;
        
        // Adjust scroll offset if needed
        if (E->completion->selected < E->popup_scroll_offset) {
            E->popup_scroll_offset = E->completion->selected;
        }
    }
}

void editor_completion_scroll_down(EditorState *E) {
    if (!E->popup_visible || !E->completion) return;
    
    if (E->completion->selected < E->completion->count - 1) {
        E->completion->selected++;
        
        // Adjust scroll offset if needed
        int visible_items = E->popup_height - 2;
        if (E->completion->selected >= E->popup_scroll_offset + visible_items) {
            E->popup_scroll_offset = E->completion->selected - visible_items + 1;
        }
    }
}

void editor_completion_page_up(EditorState *E) {
    if (!E->popup_visible || !E->completion) return;
    
    int visible_items = E->popup_height - 2;
    int new_selected = E->completion->selected - visible_items;
    if (new_selected < 0) new_selected = 0;
    
    E->completion->selected = new_selected;
    
    // Adjust scroll offset
    if (E->completion->selected < E->popup_scroll_offset) {
        E->popup_scroll_offset = E->completion->selected;
    }
}

void editor_completion_page_down(EditorState *E) {
    if (!E->popup_visible || !E->completion) return;
    
    int visible_items = E->popup_height - 2;
    int new_selected = E->completion->selected + visible_items;
    if (new_selected >= E->completion->count) new_selected = E->completion->count - 1;
    
    E->completion->selected = new_selected;
    
    // Adjust scroll offset
    if (E->completion->selected >= E->popup_scroll_offset + visible_items) {
        E->popup_scroll_offset = E->completion->selected - visible_items + 1;
    }
}

