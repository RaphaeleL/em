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
#include <dirent.h>
#include <sys/stat.h>

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

// File completion implementation
FileCompletion *file_completion_new(void) {
    FileCompletion *fc = calloc(1, sizeof(FileCompletion));
    fc->capacity = 16;
    fc->matches = calloc(fc->capacity, sizeof(char*));
    fc->count = 0;
    fc->selected = 0;
    return fc;
}

void file_completion_free(FileCompletion *fc) {
    if (!fc) return;
    for (int i = 0; i < fc->count; ++i) {
        free(fc->matches[i]);
    }
    free(fc->matches);
    free(fc);
}

// Helper function to check if a string starts with another string
static int starts_with(const char *str, const char *prefix) {
    return strncmp(str, prefix, strlen(prefix)) == 0;
}

// Helper function to get directory and filename from path
static void split_path(const char *path, char *dir, char *file, size_t dir_size, size_t file_size) {
    const char *last_slash = strrchr(path, '/');
    if (last_slash) {
        size_t dir_len = last_slash - path;
        if (dir_len >= dir_size) dir_len = dir_size - 1;
        strncpy(dir, path, dir_len);
        dir[dir_len] = '\0';
        strncpy(file, last_slash + 1, file_size - 1);
        file[file_size - 1] = '\0';
    } else {
        strncpy(dir, ".", dir_size - 1);
        dir[dir_size - 1] = '\0';
        strncpy(file, path, file_size - 1);
        file[file_size - 1] = '\0';
    }
}

int file_completion_find_matches(FileCompletion *fc, const char *pattern) {
    // Clear existing matches
    for (int i = 0; i < fc->count; ++i) {
        free(fc->matches[i]);
    }
    fc->count = 0;
    fc->selected = 0;

    // If pattern is empty, show all files in current directory
    if (!pattern || strlen(pattern) == 0) {
        pattern = "";
    }

    char dir[512];
    char file_prefix[512];
    split_path(pattern, dir, file_prefix, sizeof(dir), sizeof(file_prefix));

    DIR *d = opendir(dir);
    if (!d) {
        return 0;
    }

    // Add parent directory entry if not in root
    if (strcmp(dir, ".") != 0 && strcmp(dir, "/") != 0) {
        if (starts_with("..", file_prefix)) {
            // Ensure we have capacity
            if (fc->count >= fc->capacity) {
                fc->capacity *= 2;
                fc->matches = realloc(fc->matches, fc->capacity * sizeof(char*));
            }
            
            // Find parent directory
            char *last_slash = strrchr(dir, '/');
            char parent_path[1024];
            if (last_slash && last_slash != dir) {
                size_t parent_len = last_slash - dir;
                strncpy(parent_path, dir, parent_len);
                parent_path[parent_len] = '\0';
            } else {
                strcpy(parent_path, "..");
            }
            
            fc->matches[fc->count] = strdup(parent_path);
            fc->count++;
        }
    }

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        // Skip hidden files unless pattern starts with '.'
        if (entry->d_name[0] == '.' && file_prefix[0] != '.') {
            continue;
        }

        // Check if this entry matches our prefix
        if (starts_with(entry->d_name, file_prefix)) {
            // Ensure we have capacity
            if (fc->count >= fc->capacity) {
                fc->capacity *= 2;
                fc->matches = realloc(fc->matches, fc->capacity * sizeof(char*));
            }

            // Build full path
            char full_path[1024];
            if (strcmp(dir, ".") == 0) {
                strncpy(full_path, entry->d_name, sizeof(full_path) - 1);
            } else {
                snprintf(full_path, sizeof(full_path), "%s/%s", dir, entry->d_name);
            }
            full_path[sizeof(full_path) - 1] = '\0';

            // Check if it's a directory and add trailing slash
            struct stat st;
            if (stat(full_path, &st) == 0 && S_ISDIR(st.st_mode)) {
                size_t len = strlen(full_path);
                if (len < sizeof(full_path) - 1) {
                    full_path[len] = '/';
                    full_path[len + 1] = '\0';
                }
            }

            fc->matches[fc->count] = strdup(full_path);
            fc->count++;
        }
    }

    closedir(d);
    return fc->count;
}

char *file_completion_get_selected(FileCompletion *fc) {
    if (fc->count == 0 || fc->selected >= fc->count) {
        return NULL;
    }
    return fc->matches[fc->selected];
}

void file_completion_next(FileCompletion *fc) {
    if (fc->count > 0) {
        fc->selected = (fc->selected + 1) % fc->count;
    }
}

void file_completion_prev(FileCompletion *fc) {
    if (fc->count > 0) {
        fc->selected = (fc->selected - 1 + fc->count) % fc->count;
    }
}
