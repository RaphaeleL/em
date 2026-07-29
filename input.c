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
#include <sys/stat.h>

#include "includes/input.h"
#include "includes/config.h"

// Track the previous command so consecutive self-inserts are grouped
// into one undo step and consecutive C-k kills append to the kill buffer.
typedef enum { CMD_OTHER, CMD_INSERT, CMD_KILL } LastCmd;
static LastCmd last_cmd = CMD_OTHER;

static void kill_buf_set(EditorState *E, const char *text) {
    free(E->kill_buf);
    E->kill_buf = xstrdup(text);
}

static void kill_buf_append(EditorState *E, const char *text) {
    if (!E->kill_buf) {
        kill_buf_set(E, text);
        return;
    }
    size_t a = strlen(E->kill_buf), b = strlen(text);
    E->kill_buf = xrealloc(E->kill_buf, a + b + 1);
    memcpy(E->kill_buf + a, text, b + 1);
}

void editor_insert_char(EditorState *E, int c) {
    if (buffer_is_readonly(E->buf)) {
        editor_message(E, "Buffer is read-only");
        return;
    }
    editor_clamp_cursor(E);
    if (last_cmd != CMD_INSERT) buffer_push_undo(E->buf, E->cx, E->cy);
    last_cmd = CMD_INSERT;

    Buffer *b = E->buf;
    char *line = b->lines[E->cy];
    int llen = (int)strlen(line);
    char *newl = xmalloc(llen + 2);
    memcpy(newl, line, E->cx);
    newl[E->cx] = (char)c;
    memcpy(newl + E->cx + 1, line + E->cx, llen - E->cx + 1);
    free(b->lines[E->cy]);
    b->lines[E->cy] = newl;
    E->cx++;
    E->goal_cx = E->cx;
    b->modified = 1;
}

// Insert arbitrary text (may contain newlines) at the cursor.
static void editor_insert_text(EditorState *E, const char *text) {
    Buffer *b = E->buf;
    for (const char *p = text; *p; ++p) {
        if (*p == '\n') {
            char *line = b->lines[E->cy];
            char *right = xstrdup(line + E->cx);
            line[E->cx] = '\0';
            buffer_insert_line(b, E->cy + 1, right);
            free(right);
            E->cy++;
            E->cx = 0;
        } else {
            char *line = b->lines[E->cy];
            int llen = (int)strlen(line);
            char *newl = xmalloc(llen + 2);
            memcpy(newl, line, E->cx);
            newl[E->cx] = *p;
            memcpy(newl + E->cx + 1, line + E->cx, llen - E->cx + 1);
            free(b->lines[E->cy]);
            b->lines[E->cy] = newl;
            E->cx++;
        }
    }
    E->goal_cx = E->cx;
    b->modified = 1;
}

void editor_backspace(EditorState *E) {
    if (buffer_is_readonly(E->buf)) {
        editor_message(E, "Buffer is read-only");
        return;
    }
    editor_clamp_cursor(E);
    Buffer *b = E->buf;
    if (E->cx == 0 && E->cy == 0) return;
    buffer_push_undo(b, E->cx, E->cy);
    if (E->cx > 0) {
        char *line = b->lines[E->cy];
        int llen = (int)strlen(line);
        memmove(&line[E->cx - 1], &line[E->cx], llen - E->cx + 1);
        E->cx--;
    } else {
        // join with previous line
        int prev = E->cy - 1;
        char *p = b->lines[prev];
        char *cur = b->lines[E->cy];
        size_t plen = strlen(p);
        char *merged = xmalloc(plen + strlen(cur) + 1);
        strcpy(merged, p);
        strcat(merged, cur);
        free(b->lines[prev]);
        b->lines[prev] = merged;
        buffer_delete_line(b, E->cy);
        E->cy = prev;
        E->cx = (int)plen;
    }
    E->goal_cx = E->cx;
    b->modified = 1;
}

void editor_delete_char(EditorState *E) {
    if (buffer_is_readonly(E->buf)) {
        editor_message(E, "Buffer is read-only");
        return;
    }
    editor_clamp_cursor(E);
    Buffer *b = E->buf;
    char *line = b->lines[E->cy];
    int llen = (int)strlen(line);
    if (E->cx < llen) {
        buffer_push_undo(b, E->cx, E->cy);
        memmove(&line[E->cx], &line[E->cx + 1], llen - E->cx);
        b->modified = 1;
    } else if (E->cy + 1 < b->nlines) {
        // join with next line
        buffer_push_undo(b, E->cx, E->cy);
        char *next = b->lines[E->cy + 1];
        char *merged = xmalloc(llen + strlen(next) + 1);
        strcpy(merged, line);
        strcat(merged, next);
        free(b->lines[E->cy]);
        b->lines[E->cy] = merged;
        buffer_delete_line(b, E->cy + 1);
        b->modified = 1;
    }
}

void editor_enter(EditorState *E) {
    if (buffer_is_readonly(E->buf)) {
        editor_message(E, "Buffer is read-only");
        return;
    }
    editor_clamp_cursor(E);
    buffer_push_undo(E->buf, E->cx, E->cy);
    Buffer *b = E->buf;
    char *line = b->lines[E->cy];
    char *right = xstrdup(line + E->cx);
    line[E->cx] = '\0';
    buffer_insert_line(b, E->cy + 1, right);
    free(right);
    E->cy++;
    E->cx = 0;
    E->goal_cx = 0;
}

// ------------------------------------------------------------------
// mark / region / kill / yank

void editor_set_mark(EditorState *E) {
    editor_clamp_cursor(E);
    E->mark_x = E->cx;
    E->mark_y = E->cy;
    E->mark_active = 1;
    editor_message(E, "Mark set");
}

// Build a heap string of the region content ('\n' separated).
static char *region_to_string(EditorState *E, int sy, int sx, int ey, int ex) {
    Buffer *b = E->buf;
    size_t total = 0;
    for (int y = sy; y <= ey; ++y) {
        int len = (int)strlen(b->lines[y]);
        int from = (y == sy) ? sx : 0;
        int to = (y == ey) ? ex : len;
        total += (size_t)(to - from) + 1; // +1 for '\n' or NUL
    }
    char *out = xmalloc(total + 1);
    char *p = out;
    for (int y = sy; y <= ey; ++y) {
        int len = (int)strlen(b->lines[y]);
        int from = (y == sy) ? sx : 0;
        int to = (y == ey) ? ex : len;
        memcpy(p, b->lines[y] + from, to - from);
        p += to - from;
        if (y != ey) *p++ = '\n';
    }
    *p = '\0';
    return out;
}

static void delete_region(EditorState *E, int sy, int sx, int ey, int ex) {
    Buffer *b = E->buf;
    if (sy == ey) {
        char *line = b->lines[sy];
        int llen = (int)strlen(line);
        memmove(&line[sx], &line[ex], llen - ex + 1);
    } else {
        // keep start-line head + end-line tail, drop everything between
        char *head = b->lines[sy];
        char *tail = b->lines[ey] + ex;
        char *merged = xmalloc((size_t)sx + strlen(tail) + 1);
        memcpy(merged, head, sx);
        strcpy(merged + sx, tail);
        free(b->lines[sy]);
        b->lines[sy] = merged;
        for (int y = ey; y > sy; --y) buffer_delete_line(b, y);
    }
    E->cy = sy;
    E->cx = sx;
    E->goal_cx = sx;
    b->modified = 1;
}

// Delete-selection behavior: typing or deleting with an active region
// removes the region first. Returns 1 if a region was deleted.
static int delete_active_region(EditorState *E) {
    int sy, sx, ey, ex;
    int has = editor_region_bounds(E, &sy, &sx, &ey, &ex);
    E->mark_active = 0;
    if (!has || buffer_is_readonly(E->buf)) return 0;
    buffer_push_undo(E->buf, E->cx, E->cy);
    delete_region(E, sy, sx, ey, ex);
    return 1;
}

void editor_copy_region(EditorState *E) {
    int sy, sx, ey, ex;
    if (!editor_region_bounds(E, &sy, &sx, &ey, &ex)) {
        editor_message(E, "No region");
        return;
    }
    char *text = region_to_string(E, sy, sx, ey, ex);
    kill_buf_set(E, text);
    free(text);
    E->mark_active = 0;
    editor_message(E, "Region copied");
}

void editor_kill_region(EditorState *E) {
    int sy, sx, ey, ex;
    if (!editor_region_bounds(E, &sy, &sx, &ey, &ex)) {
        editor_message(E, "No region");
        return;
    }
    if (buffer_is_readonly(E->buf)) {
        editor_message(E, "Buffer is read-only");
        return;
    }
    char *text = region_to_string(E, sy, sx, ey, ex);
    kill_buf_set(E, text);
    free(text);
    buffer_push_undo(E->buf, E->cx, E->cy);
    delete_region(E, sy, sx, ey, ex);
    E->mark_active = 0;
    editor_message(E, "Region killed");
}

void editor_kill_line(EditorState *E) {
    if (buffer_is_readonly(E->buf)) {
        editor_message(E, "Buffer is read-only");
        return;
    }
    editor_clamp_cursor(E);
    Buffer *b = E->buf;
    char *line = b->lines[E->cy];
    int llen = (int)strlen(line);

    buffer_push_undo(b, E->cx, E->cy);
    if (E->cx < llen) {
        // kill to end of line
        if (last_cmd == CMD_KILL) kill_buf_append(E, line + E->cx);
        else kill_buf_set(E, line + E->cx);
        line[E->cx] = '\0';
        b->modified = 1;
    } else if (E->cy + 1 < b->nlines) {
        // at end of line: kill the newline (join with next line)
        if (last_cmd == CMD_KILL) kill_buf_append(E, "\n");
        else kill_buf_set(E, "\n");
        char *next = b->lines[E->cy + 1];
        char *merged = xmalloc(llen + strlen(next) + 1);
        strcpy(merged, line);
        strcat(merged, next);
        free(b->lines[E->cy]);
        b->lines[E->cy] = merged;
        buffer_delete_line(b, E->cy + 1);
        b->modified = 1;
    }
}

void editor_yank(EditorState *E) {
    if (buffer_is_readonly(E->buf)) {
        editor_message(E, "Buffer is read-only");
        return;
    }
    if (!E->kill_buf || !*E->kill_buf) {
        editor_message(E, "Kill buffer is empty");
        return;
    }
    editor_clamp_cursor(E);
    buffer_push_undo(E->buf, E->cx, E->cy);
    editor_insert_text(E, E->kill_buf);
}

void editor_undo_cmd(EditorState *E) {
    if (buffer_is_readonly(E->buf)) {
        editor_message(E, "Buffer is read-only");
        return;
    }
    int cx, cy;
    if (buffer_undo(E->buf, &cx, &cy) == 0) {
        E->cx = cx;
        E->cy = cy;
        E->goal_cx = cx;
        E->mark_active = 0;
        editor_message(E, "Undo");
    } else {
        editor_message(E, "Nothing to undo");
    }
}

// ------------------------------------------------------------------
// incremental search

// Search forward for `q` starting at (*y, *x). Returns 1 on match and
// updates (*y, *x) to the match start. Wraps around the buffer once.
static int search_forward(Buffer *b, const char *q, int *y, int *x, int *wrapped) {
    if (wrapped) *wrapped = 0;
    int startY = *y, startX = *x;
    int cy = startY, cx = startX;
    for (int pass = 0; pass < 2; ++pass) {
        while (cy < b->nlines) {
            const char *line = b->lines[cy];
            int len = (int)strlen(line);
            if (cx <= len) {
                const char *hit = strstr(line + cx, q);
                if (hit) {
                    int col = (int)(hit - line);
                    if (pass == 1 && (cy > startY || (cy == startY && col >= startX))) {
                        // completed the wrap without a new match
                        if (cy == startY && col == startX) { *y = cy; *x = col; return 1; }
                    }
                    *y = cy;
                    *x = col;
                    return 1;
                }
            }
            cy++;
            cx = 0;
        }
        // wrap
        cy = 0;
        cx = 0;
        if (wrapped) *wrapped = 1;
    }
    return 0;
}

void editor_isearch(EditorState *E) {
    char query[256] = "";
    int qlen = 0;
    int orig_cx = E->cx, orig_cy = E->cy, orig_ro = E->row_offset;
    int failing = 0, wrapped = 0;

    while (1) {
        char prompt[320];
        snprintf(prompt, sizeof(prompt), "%s%sI-search: %s",
                 failing ? "Failing " : "", wrapped ? "Wrapped " : "", query);
        snprintf(E->minibuf, sizeof(E->minibuf), "%s", prompt);
        editor_draw(E, NULL);

        int ch = getch();
        if (ch == ERR || ch == KEY_RESIZE) continue;

        if (ch == CTRL('g')) {
            // cancel: restore original position
            E->cx = orig_cx;
            E->cy = orig_cy;
            E->row_offset = orig_ro;
            E->goal_cx = orig_cx;
            editor_message(E, "Quit");
            return;
        } else if (ch == '\n' || ch == '\r' || ch == 27) {
            E->minibuf[0] = '\0';
            return;
        } else if (ch == KEY_BACKSPACE || ch == 127 || ch == 8) {
            if (qlen > 0) query[--qlen] = '\0';
            failing = 0;
            wrapped = 0;
            if (qlen > 0) {
                int y = orig_cy, x = orig_cx;
                if (search_forward(E->buf, query, &y, &x, &wrapped)) {
                    E->cy = y; E->cx = x + qlen; E->goal_cx = E->cx;
                } else failing = 1;
            } else {
                E->cx = orig_cx; E->cy = orig_cy; E->goal_cx = orig_cx;
            }
        } else if (ch == CTRL('s')) {
            // next match: search starting just past the current match
            if (qlen == 0) continue;
            int y = E->cy, x = E->cx - qlen + 1;
            if (x < 0) x = 0;
            if (search_forward(E->buf, query, &y, &x, &wrapped)) {
                E->cy = y; E->cx = x + qlen; E->goal_cx = E->cx;
                failing = 0;
            } else failing = 1;
        } else if (isprint(ch) && ch < 256 && qlen + 1 < (int)sizeof(query)) {
            query[qlen++] = (char)ch;
            query[qlen] = '\0';
            int y = E->cy, x = E->cx - (qlen - 1);
            if (x < 0) { x = 0; }
            wrapped = 0;
            if (search_forward(E->buf, query, &y, &x, &wrapped)) {
                E->cy = y; E->cx = x + qlen; E->goal_cx = E->cx;
                failing = 0;
            } else failing = 1;
        }
    }
}

// ------------------------------------------------------------------
// minibuffer
//
// One real line editor used by every prompt: cursor movement inside the
// input (arrows, C-a/C-e, C-d, C-k, C-u) and, when enabled, filename
// completion with a visible candidate list, longest-common-prefix
// completion on the first Tab, and cycling on further Tabs.

// Longest common prefix length across all completion matches.
static int completion_common_prefix(FileCompletion *fc) {
    if (fc->count == 0) return 0;
    int lcp = (int)strlen(fc->matches[0]);
    for (int i = 1; i < fc->count; ++i) {
        int j = 0;
        while (j < lcp && fc->matches[i][j] == fc->matches[0][j]) j++;
        lcp = j;
    }
    return lcp;
}

// Render "prompt + input" plus the candidate list into E->minibuf and
// position the terminal cursor at `pos` within the input.
static void minibuffer_render(EditorState *E, const char *prompt, const char *out,
                              int pos, FileCompletion *fc, int completion_active,
                              const char *note) {
    char extra[400] = "";
    if (note && *note) {
        snprintf(extra, sizeof(extra), "  [%s]", note);
    } else if (completion_active && fc && fc->count > 0) {
        // show candidates; the selected one is wrapped in [ ]
        size_t n = snprintf(extra, sizeof(extra), "  {%d/%d: ", fc->selected + 1, fc->count);
        for (int i = 0; i < fc->count && n + 4 < sizeof(extra); ++i) {
            const char *name = fc->matches[i];
            const char *base = strrchr(name, '/');
            // show basenames to keep the list short ("dir/" keeps its slash)
            if (base && base[1] != '\0') base++;
            else if (base && base != name) {
                const char *p = base - 1;
                while (p > name && p[-1] != '/') p--;
                base = p;
            } else base = name;
            n += snprintf(extra + n, sizeof(extra) - n, i == fc->selected ? "[%s] " : "%s ", base);
        }
        if (n + 1 < sizeof(extra)) snprintf(extra + n, sizeof(extra) - n, "}");
    }
    snprintf(E->minibuf, sizeof(E->minibuf), "%s%s%s", prompt, out, extra);
    editor_draw(E, NULL);

    // put the visible cursor inside the minibuffer input
    int rows = E->screen_rows - 2;
    int cx = (int)strlen(prompt) + pos;
    if (cx > E->screen_cols - 1) cx = E->screen_cols - 1;
    move(rows + 1, cx);
    refresh();
}

static int minibuffer_read(EditorState *E, const char *prompt, char *out, size_t outcap,
                           int want_completion) {
    int len = (int)strlen(out);
    int pos = len;
    int canceled = 0;
    FileCompletion *fc = want_completion ? file_completion_new() : NULL;
    int completion_active = 0;
    const char *note = NULL;

    while (1) {
        minibuffer_render(E, prompt, out, pos, fc, completion_active, note);
        note = NULL;
        int ch = getch();
        if (ch == ERR || ch == KEY_RESIZE) continue;

        if (ch == '\n' || ch == '\r') {
            // Enter accepts the highlighted candidate; a directory is
            // descended into instead of being returned.
            if (completion_active && fc && fc->count > 0) {
                char *sel = file_completion_get_selected(fc);
                if (sel && *sel) {
                    snprintf(out, outcap, "%s", sel);
                    len = pos = (int)strlen(out);
                    if (out[len - 1] == '/') {
                        file_completion_find_matches(fc, out);
                        completion_active = fc->count > 0;
                        continue;
                    }
                }
            }
            break;
        } else if (ch == CTRL('g') || ch == 27) {
            canceled = 1;
            break;
        } else if (ch == '\t' && want_completion) {
            if (!completion_active) {
                file_completion_find_matches(fc, out);
                if (fc->count == 0) {
                    note = "No match";
                } else if (fc->count == 1) {
                    snprintf(out, outcap, "%s", fc->matches[0]);
                    len = pos = (int)strlen(out);
                    // completing a directory re-opens completion inside it
                    if (len > 0 && out[len - 1] == '/') {
                        file_completion_find_matches(fc, out);
                        completion_active = fc->count > 0;
                    }
                } else {
                    // several matches: complete the common prefix and show them
                    int lcp = completion_common_prefix(fc);
                    if (lcp > len && lcp + 1 < (int)outcap) {
                        memcpy(out, fc->matches[0], lcp);
                        out[lcp] = '\0';
                        len = pos = lcp;
                    }
                    completion_active = 1;
                }
            } else {
                file_completion_next(fc);
                char *sel = file_completion_get_selected(fc);
                if (sel) {
                    snprintf(out, outcap, "%s", sel);
                    len = pos = (int)strlen(out);
                }
            }
        } else if (ch == KEY_BTAB && want_completion && completion_active) {
            file_completion_prev(fc);
            char *sel = file_completion_get_selected(fc);
            if (sel) {
                snprintf(out, outcap, "%s", sel);
                len = pos = (int)strlen(out);
            }
        } else if (ch == KEY_LEFT || ch == CTRL('b')) {
            if (pos > 0) pos--;
        } else if (ch == KEY_RIGHT || ch == CTRL('f')) {
            if (pos < len) pos++;
        } else if (ch == KEY_HOME || ch == CTRL('a')) {
            pos = 0;
        } else if (ch == KEY_END || ch == CTRL('e')) {
            pos = len;
        } else if (ch == KEY_BACKSPACE || ch == 127 || ch == 8) {
            if (pos > 0) {
                memmove(out + pos - 1, out + pos, len - pos + 1);
                pos--;
                len--;
                completion_active = 0;
            }
        } else if (ch == KEY_DC || ch == CTRL('d')) {
            if (pos < len) {
                memmove(out + pos, out + pos + 1, len - pos);
                len--;
                completion_active = 0;
            }
        } else if (ch == CTRL('k')) {
            out[pos] = '\0';
            len = pos;
            completion_active = 0;
        } else if (ch == CTRL('u')) {
            out[0] = '\0';
            len = pos = 0;
            completion_active = 0;
        } else if (isprint(ch) && ch < 256) {
            if (len + 1 < (int)outcap) {
                memmove(out + pos + 1, out + pos, len - pos + 1);
                out[pos] = (char)ch;
                pos++;
                len++;
                completion_active = 0;
            }
        }
    }
    E->minibuf[0] = '\0';
    if (fc) file_completion_free(fc);
    return canceled ? -1 : 0;
}

int editor_minibuffer_getline(EditorState *E, const char *prompt, char *out, size_t outcap) {
    return minibuffer_read(E, prompt, out, outcap, 0);
}

int editor_minibuffer_getline_with_completion(EditorState *E, const char *prompt, char *out, size_t outcap) {
    return minibuffer_read(E, prompt, out, outcap, 1);
}

// ------------------------------------------------------------------
// command system

void editor_command_mode(EditorState *E) {
    char command[256] = "";
    if (editor_minibuffer_getline(E, "M-x ", command, sizeof(command)) == 0) {
        editor_execute_command(E, command);
    }
}

void editor_execute_command(EditorState *E, const char *command) {
    if (strcmp(command, "help") == 0) {
        editor_show_help(E);
    } else if (command[0] == '\0') {
        E->minibuf[0] = '\0';
    } else {
        editor_message(E, "Unknown command: %s", command);
    }
}

void editor_show_help(EditorState *E) {
    if (buffer_load_file(E->buf, "em.hlp") == 0) {
        buffer_set_readonly(E->buf, 1);
        E->cx = E->cy = E->row_offset = E->goal_cx = 0;
        E->mark_active = 0;
        editor_message(E, "Help loaded (read-only). Press C-x C-c to exit help.");
    } else {
        editor_message(E, "Help file 'em.hlp' not found. See README for key bindings.");
    }
}

// ------------------------------------------------------------------
// save helpers

// Expand a leading "~/" (or bare "~") to $HOME.
static void expand_tilde(const char *in, char *out, size_t outcap) {
    if (in[0] == '~' && (in[1] == '/' || in[1] == '\0')) {
        const char *home = getenv("HOME");
        if (home && *home) {
            snprintf(out, outcap, "%s%s", home, in + 1);
            return;
        }
    }
    snprintf(out, outcap, "%s", in);
}

static void editor_save(EditorState *E) {
    if (!E->buf->filename || !E->buf->filename[0]) {
        char input[256] = "";
        if (editor_minibuffer_getline_with_completion(E, "Save as: ", input, sizeof(input)) != 0
            || !input[0]) {
            editor_message(E, "Save canceled");
            return;
        }
        char fname[512];
        expand_tilde(input, fname, sizeof(fname));
        if (fname[strlen(fname) - 1] == '/') {
            editor_message(E, "'%s' is a directory", fname);
            return;
        }
        if (buffer_save_file(E->buf, fname) == 0) editor_message(E, "Saved '%s'", fname);
        else editor_message(E, "Save failed: %s", strerror(errno));
    } else {
        if (buffer_save_file(E->buf, E->buf->filename) == 0)
            editor_message(E, "Saved '%s'", E->buf->filename);
        else editor_message(E, "Save failed: %s", strerror(errno));
    }
}

static void editor_reset_view(EditorState *E) {
    E->cx = E->cy = E->row_offset = E->col_offset = E->goal_cx = 0;
    E->mark_active = 0;
}

// Visit a path: a directory opens as a dired listing, an existing file
// is loaded, and a missing file starts a fresh buffer bound to that
// name (like Emacs find-file).
void editor_visit_path(EditorState *E, const char *path) {
    char fname[512];
    expand_tilde(path, fname, sizeof(fname));

    struct stat st;
    if (stat(fname, &st) == 0 && S_ISDIR(st.st_mode)) {
        if (buffer_load_dir(E->buf, fname) == 0) {
            editor_reset_view(E);
            E->cy = 2; // first entry
            editor_message(E, "Dired: RET opens, ^ parent, g refresh");
        } else {
            editor_message(E, "Cannot list '%s': %s", fname, strerror(errno));
        }
        return;
    }

    if (buffer_load_file(E->buf, fname) == 0) {
        buffer_set_readonly(E->buf, 0);
        editor_reset_view(E);
        editor_message(E, "Opened '%s'", fname);
        return;
    }

    if (errno == ENOENT) {
        Buffer *fresh = buffer_new();
        fresh->filename = xstrdup(fname);
        buffer_free(E->buf);
        E->buf = fresh;
        editor_reset_view(E);
        editor_message(E, "(New file) %s", fname);
    } else {
        editor_message(E, "Open failed: %s", strerror(errno));
    }
}

// C-x C-d / C-x d: prompt for a directory and open it in dired.
// The prompt is prefilled with the current file's directory.
static void editor_dired_prompt(EditorState *E) {
    char input[256] = "";
    if (E->buf->is_dired && E->buf->filename) {
        snprintf(input, sizeof(input), "%s/", E->buf->filename);
    } else if (E->buf->filename && E->buf->filename[0]) {
        const char *slash = strrchr(E->buf->filename, '/');
        if (slash) {
            int n = (int)(slash - E->buf->filename) + 1;
            if (n > (int)sizeof(input) - 1) n = (int)sizeof(input) - 1;
            memcpy(input, E->buf->filename, n);
            input[n] = '\0';
        }
    }
    if (editor_minibuffer_getline_with_completion(E, "Dired (directory): ", input, sizeof(input)) != 0) {
        editor_message(E, "Dired canceled");
        return;
    }
    if (!input[0]) snprintf(input, sizeof(input), ".");

    if (E->buf->modified) {
        char ans[10] = "";
        if (editor_minibuffer_getline(E, "Buffer modified; discard changes? (y/N) ", ans, sizeof(ans)) != 0
            || (ans[0] != 'y' && ans[0] != 'Y')) {
            editor_message(E, "Dired canceled");
            return;
        }
    }

    char fname[512];
    expand_tilde(input, fname, sizeof(fname));
    struct stat st;
    if (stat(fname, &st) != 0 || !S_ISDIR(st.st_mode)) {
        editor_message(E, "Not a directory: %s", fname);
        return;
    }
    editor_visit_path(E, fname);
}

// C-x C-f
static void editor_find_file(EditorState *E) {
    char input[256] = "";
    if (editor_minibuffer_getline_with_completion(E, "Open file: ", input, sizeof(input)) != 0 || !input[0]) {
        editor_message(E, "Open canceled");
        return;
    }

    if (E->buf->modified) {
        char ans[10] = "";
        if (editor_minibuffer_getline(E, "Buffer modified; discard changes? (y/N) ", ans, sizeof(ans)) != 0
            || (ans[0] != 'y' && ans[0] != 'Y')) {
            editor_message(E, "Open canceled");
            return;
        }
    }

    editor_visit_path(E, input);
}

// ------------------------------------------------------------------
// dired

// Entry name on the cursor line of a dired buffer, or NULL.
// Lines look like: "-rw-r--r-- 1 user staff 1234 Jul 29 15:12 name"
// so the name is everything after the first 8 fields. For symlinks
// the " -> target" suffix is stripped.
static const char *dired_entry_at_cursor(EditorState *E) {
    static char name[512];
    if (E->cy < 2 || E->cy >= E->buf->nlines) return NULL;
    const char *line = E->buf->lines[E->cy];
    if (!*line) return NULL;

    const char *p = line;
    for (int field = 0; field < 8; ++field) {
        while (*p == ' ') p++;
        if (!*p) return NULL;
        while (*p && *p != ' ') p++;
    }
    while (*p == ' ') p++;
    if (!*p) return NULL;

    snprintf(name, sizeof(name), "%s", p);
    if (line[0] == 'l') {
        char *arrow = strstr(name, " -> ");
        if (arrow) *arrow = '\0';
    }
    return name;
}

// Handle a key in a dired buffer. Returns 1 if the key was consumed.
static int editor_dired_key(EditorState *E, int c) {
    if (c == '\n' || c == '\r' || c == 'f' || c == 'e') {
        const char *entry = dired_entry_at_cursor(E);
        if (!entry) {
            editor_message(E, "No file on this line");
            return 1;
        }
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", E->buf->filename, entry);
        editor_visit_path(E, path);
        return 1;
    } else if (c == '^') {
        char path[1024];
        snprintf(path, sizeof(path), "%s/..", E->buf->filename);
        editor_visit_path(E, path);
        return 1;
    } else if (c == 'g') {
        char dir[1024];
        snprintf(dir, sizeof(dir), "%s", E->buf->filename);
        int cy = E->cy;
        if (buffer_load_dir(E->buf, dir) == 0) {
            editor_reset_view(E);
            E->cy = cy < E->buf->nlines ? cy : E->buf->nlines - 1;
            if (E->cy < 2) E->cy = 2;
            editor_message(E, "Refreshed");
        }
        return 1;
    } else if (c == 'n') {
        editor_move_cursor_down(E);
        return 1;
    } else if (c == 'p') {
        editor_move_cursor_up(E);
        return 1;
    } else if (c == 'q') {
        editor_message(E, "Dired: RET opens, ^ parent, g refresh, C-x C-c quits");
        return 1;
    }
    return 0;
}

static void editor_quit(EditorState *E) {
    if (E->buf->modified) {
        char ans[10] = "";
        if (editor_minibuffer_getline(E, "Modified; save before exit? (y/N) ", ans, sizeof(ans)) == 0) {
            if (ans[0] == 'y' || ans[0] == 'Y') {
                editor_save(E);
            }
        } else {
            editor_message(E, "Quit canceled");
            return; // C-g during the prompt cancels quitting
        }
    }
    endwin();
    buffer_free(E->buf);
    free(E->kill_buf);
    exit(0);
}

// ------------------------------------------------------------------
// main key dispatch

// Read the key following ESC without blocking forever: a lone ESC press
// yields ERR after a short timeout instead of hanging the editor.
static int getch_after_esc(void) {
    timeout(50);
    int c = getch();
    timeout(-1);
    return c;
}

static void editor_handle_meta(EditorState *E, int c) {
    switch (c) {
        case 'f': editor_move_cursor_forward_word(E); break;
        case 'b': editor_move_cursor_backward_word(E); break;
        case 'v': editor_scroll_page_up(E); break;
        case 'w': editor_copy_region(E); break;
        case '<': editor_move_to_buffer_start(E); break;
        case '>': editor_move_to_buffer_end(E); break;
        case 'x': editor_command_mode(E); break;
        default:
            editor_message(E, "M-%c is undefined", isprint(c) ? c : '?');
            break;
    }
}

static void editor_handle_cx_prefix(EditorState *E) {
    editor_message(E, "C-x -");
    int c2 = getch();
    E->minibuf[0] = '\0';
    if (c2 == ERR || c2 == KEY_RESIZE) return;
    if (c2 == CTRL('s')) {
        editor_save(E);
    } else if (c2 == CTRL('f')) {
        editor_find_file(E);
    } else if (c2 == CTRL('d') || c2 == 'd') {
        editor_dired_prompt(E);
    } else if (c2 == CTRL('c')) {
        editor_quit(E);
    } else if (c2 == CTRL('x')) {
        // exchange point and mark
        if (E->mark_active) {
            int tx = E->cx, ty = E->cy;
            E->cx = E->mark_x; E->cy = E->mark_y;
            E->mark_x = tx; E->mark_y = ty;
            E->goal_cx = E->cx;
        } else {
            editor_message(E, "No mark set");
        }
    } else if (c2 == CTRL('g')) {
        editor_message(E, "Quit");
    } else {
        editor_message(E, "C-x %c is undefined", isprint(c2) ? c2 : '?');
    }
}

void editor_process_key(EditorState *E) {
    int c = getch();
    if (c == ERR) return;

    if (E->buf->is_dired && editor_dired_key(E, c)) {
        last_cmd = CMD_OTHER;
        return;
    }

    LastCmd this_cmd = CMD_OTHER;

    switch (c) {
        case KEY_RESIZE:
            // editor_draw() re-reads the terminal size
            break;

        case 27: { // ESC: meta prefix or lone escape
            int c2 = getch_after_esc();
            if (c2 == ERR || c2 == KEY_RESIZE) {
                // lone ESC: cancel mark / message
                E->mark_active = 0;
                E->minibuf[0] = '\0';
            } else if (c2 == 27) {
                // ESC ESC: swallow, cancel
                E->mark_active = 0;
                E->minibuf[0] = '\0';
            } else {
                editor_handle_meta(E, c2);
            }
            break;
        }

        case CTRL('x'):
            editor_handle_cx_prefix(E);
            break;

        case 0: // C-space (NUL): set mark
            editor_set_mark(E);
            break;

        case KEY_LEFT: case CTRL('b'): editor_move_cursor_left(E); break;
        case KEY_RIGHT: case CTRL('f'): editor_move_cursor_right(E); break;
        case KEY_UP: case CTRL('p'): editor_move_cursor_up(E); break;
        case KEY_DOWN: case CTRL('n'): editor_move_cursor_down(E); break;
        case KEY_HOME: case CTRL('a'): editor_move_cursor_to_beginning_of_line(E); break;
        case KEY_END: case CTRL('e'): editor_move_cursor_to_end_of_line(E); break;
        case KEY_NPAGE: case CTRL('v'): editor_scroll_page_down(E); break;
        case KEY_PPAGE: editor_scroll_page_up(E); break;

        case KEY_SLEFT: // shift-arrows extend the selection
            if (!E->mark_active) { E->mark_active = 1; E->mark_x = E->cx; E->mark_y = E->cy; }
            editor_move_cursor_left(E);
            break;
        case KEY_SRIGHT:
            if (!E->mark_active) { E->mark_active = 1; E->mark_x = E->cx; E->mark_y = E->cy; }
            editor_move_cursor_right(E);
            break;

        case CTRL('l'): editor_recenter(E); break;
        case CTRL('s'): editor_isearch(E); break;
        case CTRL('w'): editor_kill_region(E); break;
        case CTRL('y'):
            if (E->mark_active) delete_active_region(E);
            editor_yank(E);
            break;
        case CTRL('k'):
            editor_kill_line(E);
            this_cmd = CMD_KILL;
            break;
        case CTRL('d'): case KEY_DC:
            if (E->mark_active && delete_active_region(E)) break;
            editor_delete_char(E);
            break;
        case CTRL('_'): // C-_ and C-/ both arrive as 0x1F: undo
            editor_undo_cmd(E);
            break;

        case KEY_BACKSPACE: case 127: case 8:
            if (E->mark_active && delete_active_region(E)) break;
            editor_backspace(E);
            break;

        case '\r': case '\n':
            if (E->mark_active) delete_active_region(E);
            editor_enter(E);
            break;

        case CTRL('g'): // cancel: deactivate mark, clear message
            E->mark_active = 0;
            E->minibuf[0] = '\0';
            break;

        default:
            if (c == '\t' || (c >= 32 && c < 127)) {
                if (E->mark_active && delete_active_region(E)) {
                    // group the region deletion and the insert into one
                    // undo step: the insert must not push another snapshot
                    last_cmd = CMD_INSERT;
                }
                if (c == '\t') {
                    for (int i = 0; i < TAB_WIDTH; ++i) editor_insert_char(E, ' ');
                } else {
                    editor_insert_char(E, c);
                }
                this_cmd = CMD_INSERT;
            }
            // anything else (unknown function keys, bytes >= 127) is ignored
            break;
    }

    last_cmd = this_cmd;
}
