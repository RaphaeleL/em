/* 
 * input.c
 *
 * Handles user input and key bindings for the text editor.
 *
 * Created at:  12. Sep 2025 
 * Author:      Raphaele Salvatore Licciardo 
 *
 *
 * Copyright (c) 2025 Raphaele Salvatore Licciardo
 *
 */

#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#include "includes/input.h"
#include "includes/config.h"

void editor_insert_char(EditorState *E, int c) {
    Buffer *b = E->buf;
    char *line = b->lines[E->cy];
    int llen = strlen(line);
    char *newl = malloc(llen + 2);
    memcpy(newl, line, E->cx);
    newl[E->cx] = (char)c;
    memcpy(newl + E->cx + 1, line + E->cx, llen - E->cx + 1);
    free(b->lines[E->cy]);
    b->lines[E->cy] = newl;
    E->cx++;
    b->modified = 1;
}

void editor_backspace(EditorState *E) {
    Buffer *b = E->buf;
    if (E->cx > 0) {
        char *line = b->lines[E->cy];
        int llen = strlen(line);
        memmove(&line[E->cx - 1], &line[E->cx], llen - E->cx + 1);
        E->cx--;
        b->modified = 1;
    } else if (E->cy > 0) {
        // join with previous line
        int prev = E->cy - 1;
        char *p = b->lines[prev];
        char *cur = b->lines[E->cy];
        int newlen = strlen(p) + strlen(cur);
        char *merged = malloc(newlen + 1);
        strcpy(merged, p);
        strcat(merged, cur);
        free(b->lines[prev]);
        b->lines[prev] = merged;
        buffer_delete_line(b, E->cy);
        E->cy = prev;
        E->cx = strlen(p);
        if (E->cy < E->row_offset) E->row_offset = E->cy;
        b->modified = 1;
    }
}

void editor_enter(EditorState *E) {
    Buffer *b = E->buf;
    char *line = b->lines[E->cy];
    // split at cursor
    char *left = malloc(E->cx + 1);
    char *right = strdup(line + E->cx);
    memcpy(left, line, E->cx);
    left[E->cx] = '\0';
    free(b->lines[E->cy]);
    b->lines[E->cy] = left;
    buffer_insert_line(b, E->cy + 1, right);
    free(right);
    E->cy++;
    E->cx = 0;
    int rows = E->screen_rows - 2;
    if (E->cy >= E->row_offset + rows) E->row_offset = E->cy - rows + 1;
}

// minibuffer GET LINE: returns 0 on success, -1 on cancel
int editor_minibuffer_getline(EditorState *E, const char *prompt, char *out, size_t outcap) {
    E->minibuf[0] = '\0';
    int pos = 0;
    int done = 0;
    int canceled = 0;
    while (!done) {
        // show prompt + current input
        snprintf(E->minibuf, sizeof(E->minibuf), "%s%s", prompt, out);
        editor_draw(E, NULL);
        int ch = getch();
        if (ch == '\n' || ch == '\r') {
            done = 1;
        } else if (ch == KEY_BACKSPACE || ch == 127 || ch == 8) {
            if (pos > 0) {
                pos--;
                out[pos] = '\0';
            }
        } else if (ch == CTRL('g')) {
            canceled = 1;
            break;
        } else if (isprint(ch)) {
            if (pos + 1 < (int)outcap) {
                out[pos++] = (char)ch;
                out[pos] = '\0';
            }
        }
    }
    E->minibuf[0] = '\0';
    return canceled ? -1 : 0;
}

void editor_process_key(EditorState *E) {
    int c = getch();
    // handle prefix C-x sequences
    if (c == CTRL('x')) {
        int c2 = getch();
        if (c2 == CTRL('s')) { // save
            if (!E->buf->filename) {
                char fname[256] = "";
                if (editor_minibuffer_getline(E, "Save as: ", fname, sizeof(fname)) == 0) {
                    if (buffer_save_file(E->buf, fname) == 0) {
                        editor_message(E, "Saved '%s'", fname);
                    } else {
                        editor_message(E, "Save failed: %s", strerror(errno));
                    }
                } else {
                    editor_message(E, "Save canceled");
                }
            } else {
                if (buffer_save_file(E->buf, E->buf->filename) == 0) {
                    editor_message(E, "Saved '%s'", E->buf->filename);
                } else {
                    editor_message(E, "Save failed: %s", strerror(errno));
                }
            }
        } else if (c2 == CTRL('f')) { // find / open
            char fname[256] = "";
            if (editor_minibuffer_getline(E, "Open file: ", fname, sizeof(fname)) == 0) {
                if (buffer_load_file(E->buf, fname) == 0) {
                    E->cx = E->cy = E->row_offset = 0;
                    editor_message(E, "Opened '%s'", fname);
                } else {
                    editor_message(E, "Open failed: %s", strerror(errno));
                }
            } else {
                editor_message(E, "Open canceled");
            }
        } else if (c2 == CTRL('c')) { // exit
            if (E->buf->modified) {
                char ans[10] = "";
                if (editor_minibuffer_getline(E, "Modified; save before exit? (y/N) ", ans, sizeof(ans)) == 0) {
                    if (ans[0] == 'y' || ans[0] == 'Y') {
                        if (!E->buf->filename) {
                            char fname[256] = "";
                            if (editor_minibuffer_getline(E, "Save as: ", fname, sizeof(fname)) == 0) {
                                buffer_save_file(E->buf, fname);
                            }
                        } else {
                            buffer_save_file(E->buf, E->buf->filename);
                        }
                    }
                    // then exit anyway
                }
            }
            // exit program
            endwin();
            buffer_free(E->buf);
            exit(0);
        } else {
            editor_message(E, "Unknown C-x sequence");
        }
        return;
    }

    // navigation keys and editing
    switch (c) {
        case KEY_LEFT: editor_move_cursor_left(E); break;
        case KEY_RIGHT: editor_move_cursor_right(E); break;
        case KEY_UP: editor_move_cursor_up(E); break;
        case KEY_DOWN: editor_move_cursor_down(E); break;
        case CTRL('b'): editor_move_cursor_left(E); break;
        case CTRL('f'): editor_move_cursor_right(E); break;
        case CTRL('p'): editor_move_cursor_up(E); break;
        case CTRL('n'): editor_move_cursor_down(E); break;
        case KEY_BACKSPACE:
        case 127:
        case 8:
            editor_backspace(E);
            break;
        case '\r':
        case '\n':
            editor_enter(E);
            break;
        case CTRL('s'): // quick save: behave like C-x C-s (but no prefix)
            if (!E->buf->filename) {
                char fname[256] = "";
                if (editor_minibuffer_getline(E, "Save as: ", fname, sizeof(fname)) == 0) {
                    if (buffer_save_file(E->buf, fname) == 0) {
                        editor_message(E, "Saved '%s'", fname);
                    } else {
                        editor_message(E, "Save failed: %s", strerror(errno));
                    }
                }
            } else {
                if (buffer_save_file(E->buf, E->buf->filename) == 0) {
                    editor_message(E, "Saved '%s'", E->buf->filename);
                } else {
                    editor_message(E, "Save failed: %s", strerror(errno));
                }
            }
            break;
        case CTRL('g'): // cancel / clear message
            E->minibuf[0] = '\0';
            break;
        default:
            if (isprint(c) || c == '\t') {
                if (c == '\t') {
                    for (int i = 0; i < TAB_WIDTH; ++i) editor_insert_char(E, ' ');
                } else {
                    editor_insert_char(E, c);
                }
            }
            break;
    }
}

