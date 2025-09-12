/* 
 * buffer.c
 *
 * Minimal text buffer implementation for a simple text editor.
 *
 * Created at:  12. Sep 2025 
 * Author:      Raphaele Salvatore Licciardo 
 *
 *
 * Copyright (c) 2025 Raphaele Salvatore Licciardo
 *
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "includes/buffer.h"

Buffer *buffer_new(void) {
    Buffer *b = calloc(1, sizeof(Buffer));
    b->capacity = 16;
    b->lines = calloc(b->capacity, sizeof(char*));
    b->nlines = 1;
    b->lines[0] = strdup("");
    b->modified = 0;
    b->filename = NULL;
    return b;
}

void buffer_free(Buffer *b) {
    if (!b) return;
    for (int i = 0; i < b->nlines; ++i) free(b->lines[i]);
    free(b->lines);
    free(b->filename);
    free(b);
}

void buffer_ensure_capacity(Buffer *b, int newcap) {
    if (newcap <= b->capacity) return;
    while (b->capacity < newcap) b->capacity *= 2;
    b->lines = realloc(b->lines, b->capacity * sizeof(char*));
}

void buffer_insert_line(Buffer *b, int idx, const char *s) {
    if (idx < 0) idx = 0;
    if (idx > b->nlines) idx = b->nlines;
    buffer_ensure_capacity(b, b->nlines + 1);
    memmove(&b->lines[idx + 1], &b->lines[idx], (b->nlines - idx) * sizeof(char*));
    b->lines[idx] = strdup(s ? s : "");
    b->nlines++;
    b->modified = 1;
}

void buffer_delete_line(Buffer *b, int idx) {
    if (b->nlines <= 1) {
        // keep at least one empty line
        free(b->lines[0]);
        b->lines[0] = strdup("");
        b->nlines = 1;
        b->modified = 1;
        return;
    }
    free(b->lines[idx]);
    memmove(&b->lines[idx], &b->lines[idx + 1], (b->nlines - idx - 1) * sizeof(char*));
    b->nlines--;
    b->modified = 1;
}

int buffer_load_file(Buffer *b, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    // clear buffer
    for (int i = 0; i < b->nlines; ++i) free(b->lines[i]);
    b->nlines = 0;

    size_t cap = 0;
    char *line = NULL;
    ssize_t len;
    while ((len = getline(&line, &cap, f)) != -1) {
        // remove newline
        if (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
            while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
                line[--len] = '\0';
            }
        }
        buffer_ensure_capacity(b, b->nlines + 1);
        b->lines[b->nlines++] = strdup(line);
    }
    free(line);
    fclose(f);
    free(b->filename);
    b->filename = strdup(path);
    b->modified = 0;
    if (b->nlines == 0) {
        buffer_insert_line(b, 0, "");
    }
    return 0;
}

int buffer_save_file(Buffer *b, const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    for (int i = 0; i < b->nlines; ++i) {
        fputs(b->lines[i], f);
        if (i != b->nlines - 1) fputc('\n', f);
    }
    fclose(f);
    free(b->filename);
    b->filename = strdup(path);
    b->modified = 0;
    return 0;
}
