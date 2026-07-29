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
#include <limits.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <unistd.h>

#include "includes/buffer.h"

#define UNDO_MAX_DEPTH 256

void *xmalloc(size_t n) {
    void *p = malloc(n);
    if (!p) {
        fprintf(stderr, "em: out of memory\n");
        abort();
    }
    return p;
}

void *xrealloc(void *p, size_t n) {
    void *q = realloc(p, n);
    if (!q) {
        fprintf(stderr, "em: out of memory\n");
        abort();
    }
    return q;
}

char *xstrdup(const char *s) {
    char *d = strdup(s ? s : "");
    if (!d) {
        fprintf(stderr, "em: out of memory\n");
        abort();
    }
    return d;
}

Buffer *buffer_new(void) {
    Buffer *b = xmalloc(sizeof(Buffer));
    memset(b, 0, sizeof(Buffer));
    b->capacity = 16;
    b->lines = xmalloc(b->capacity * sizeof(char*));
    b->nlines = 1;
    b->lines[0] = xstrdup("");
    return b;
}

static void undo_state_free(UndoState *u) {
    if (!u) return;
    for (int i = 0; i < u->nlines; ++i) free(u->lines[i]);
    free(u->lines);
    free(u);
}

void buffer_clear_undo(Buffer *b) {
    UndoState *u = b->undo_stack;
    while (u) {
        UndoState *next = u->next;
        undo_state_free(u);
        u = next;
    }
    b->undo_stack = NULL;
    b->undo_depth = 0;
}

void buffer_push_undo(Buffer *b, int cx, int cy) {
    UndoState *u = xmalloc(sizeof(UndoState));
    u->nlines = b->nlines;
    u->cx = cx;
    u->cy = cy;
    u->lines = xmalloc(b->nlines * sizeof(char*));
    for (int i = 0; i < b->nlines; ++i) u->lines[i] = xstrdup(b->lines[i]);
    u->next = b->undo_stack;
    b->undo_stack = u;
    b->undo_depth++;

    if (b->undo_depth > UNDO_MAX_DEPTH) {
        // drop the oldest snapshot
        UndoState *cur = b->undo_stack;
        while (cur->next && cur->next->next) cur = cur->next;
        undo_state_free(cur->next);
        cur->next = NULL;
        b->undo_depth--;
    }
}

int buffer_undo(Buffer *b, int *cx, int *cy) {
    UndoState *u = b->undo_stack;
    if (!u) return -1;
    b->undo_stack = u->next;
    b->undo_depth--;

    for (int i = 0; i < b->nlines; ++i) free(b->lines[i]);
    buffer_ensure_capacity(b, u->nlines);
    for (int i = 0; i < u->nlines; ++i) b->lines[i] = u->lines[i];
    b->nlines = u->nlines;
    b->modified = 1;
    if (cx) *cx = u->cx;
    if (cy) *cy = u->cy;

    free(u->lines);
    free(u);
    return 0;
}

void buffer_free(Buffer *b) {
    if (!b) return;
    buffer_clear_undo(b);
    for (int i = 0; i < b->nlines; ++i) free(b->lines[i]);
    free(b->lines);
    free(b->filename);
    free(b);
}

void buffer_ensure_capacity(Buffer *b, int newcap) {
    if (newcap <= b->capacity) return;
    while (b->capacity < newcap) b->capacity *= 2;
    b->lines = xrealloc(b->lines, b->capacity * sizeof(char*));
}

void buffer_insert_line(Buffer *b, int idx, const char *s) {
    if (idx < 0) idx = 0;
    if (idx > b->nlines) idx = b->nlines;
    buffer_ensure_capacity(b, b->nlines + 1);
    memmove(&b->lines[idx + 1], &b->lines[idx], (b->nlines - idx) * sizeof(char*));
    b->lines[idx] = xstrdup(s);
    b->nlines++;
    b->modified = 1;
}

void buffer_delete_line(Buffer *b, int idx) {
    if (idx < 0 || idx >= b->nlines) return;
    if (b->nlines <= 1) {
        // keep at least one empty line
        free(b->lines[0]);
        b->lines[0] = xstrdup("");
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
    buffer_clear_undo(b);

    size_t cap = 0;
    char *line = NULL;
    ssize_t len;
    while ((len = getline(&line, &cap, f)) != -1) {
        // remove newline
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
            line[--len] = '\0';
        }
        buffer_ensure_capacity(b, b->nlines + 1);
        b->lines[b->nlines++] = xstrdup(line);
    }
    free(line);
    fclose(f);
    free(b->filename);
    b->filename = xstrdup(path);
    b->modified = 0;
    b->is_dired = 0;
    if (b->nlines == 0) {
        buffer_insert_line(b, 0, "");
        b->modified = 0;
    }
    return 0;
}

// dired: one directory entry with everything needed for an ls -al line
typedef struct {
    char *name;
    char *link_target;   // NULL unless a symlink
    char perms[12];
    char owner[64];
    char group[64];
    char mtime[24];
    long nlink;
    long long size;
    long long blocks;
} DiredEnt;

static int dired_entry_cmp(const void *a, const void *b) {
    return strcmp(((const DiredEnt *)a)->name, ((const DiredEnt *)b)->name);
}

static void dired_format_perms(mode_t m, char *out) {
    out[0] = S_ISDIR(m)  ? 'd' :
             S_ISLNK(m)  ? 'l' :
             S_ISCHR(m)  ? 'c' :
             S_ISBLK(m)  ? 'b' :
             S_ISFIFO(m) ? 'p' :
             S_ISSOCK(m) ? 's' : '-';
    out[1] = (m & S_IRUSR) ? 'r' : '-';
    out[2] = (m & S_IWUSR) ? 'w' : '-';
    out[3] = (m & S_ISUID) ? ((m & S_IXUSR) ? 's' : 'S')
                           : ((m & S_IXUSR) ? 'x' : '-');
    out[4] = (m & S_IRGRP) ? 'r' : '-';
    out[5] = (m & S_IWGRP) ? 'w' : '-';
    out[6] = (m & S_ISGID) ? ((m & S_IXGRP) ? 's' : 'S')
                           : ((m & S_IXGRP) ? 'x' : '-');
    out[7] = (m & S_IROTH) ? 'r' : '-';
    out[8] = (m & S_IWOTH) ? 'w' : '-';
    out[9] = (m & S_ISVTX) ? ((m & S_IXOTH) ? 't' : 'T')
                           : ((m & S_IXOTH) ? 'x' : '-');
    out[10] = '\0';
}

static void dired_format_mtime(time_t t, char *out, size_t outcap) {
    time_t now = time(NULL);
    const long six_months = 15552000; // ~180 days, same cutoff as ls
    struct tm *tm = localtime(&t);
    if (!tm) {
        snprintf(out, outcap, "%12s", "?");
        return;
    }
    if (t > now || now - t > six_months) {
        strftime(out, outcap, "%b %e  %Y", tm);
    } else {
        strftime(out, outcap, "%b %e %H:%M", tm);
    }
}

static int num_width(long long v) {
    int w = 1;
    while (v >= 10) { v /= 10; w++; }
    return w;
}

int buffer_load_dir(Buffer *b, const char *path) {
    char real[PATH_MAX];
    if (!realpath(path, real)) return -1;
    DIR *d = opendir(real);
    if (!d) return -1;

    int cap = 32, n = 0;
    DiredEnt *ents = xmalloc(cap * sizeof(DiredEnt));
    long long total_blocks = 0;
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        char full[PATH_MAX + 512];
        snprintf(full, sizeof(full), "%s/%s", real, entry->d_name);
        struct stat st;
        if (lstat(full, &st) != 0) continue;

        if (n >= cap) {
            cap *= 2;
            ents = xrealloc(ents, cap * sizeof(DiredEnt));
        }
        DiredEnt *e = &ents[n++];
        memset(e, 0, sizeof(*e));
        e->name = xstrdup(entry->d_name);
        e->nlink = (long)st.st_nlink;
        e->size = (long long)st.st_size;
        e->blocks = (long long)st.st_blocks;
        total_blocks += e->blocks;
        dired_format_perms(st.st_mode, e->perms);
        dired_format_mtime(st.st_mtime, e->mtime, sizeof(e->mtime));

        struct passwd *pw = getpwuid(st.st_uid);
        if (pw) snprintf(e->owner, sizeof(e->owner), "%s", pw->pw_name);
        else snprintf(e->owner, sizeof(e->owner), "%ld", (long)st.st_uid);
        struct group *gr = getgrgid(st.st_gid);
        if (gr) snprintf(e->group, sizeof(e->group), "%s", gr->gr_name);
        else snprintf(e->group, sizeof(e->group), "%ld", (long)st.st_gid);

        if (S_ISLNK(st.st_mode)) {
            char target[PATH_MAX];
            ssize_t tl = readlink(full, target, sizeof(target) - 1);
            if (tl >= 0) {
                target[tl] = '\0';
                e->link_target = xstrdup(target);
            }
        }
    }
    closedir(d);
    qsort(ents, n, sizeof(DiredEnt), dired_entry_cmp);

    // column widths, ls-style
    int w_nlink = 1, w_owner = 1, w_group = 1, w_size = 1;
    for (int i = 0; i < n; ++i) {
        int w;
        w = num_width(ents[i].nlink);          if (w > w_nlink) w_nlink = w;
        w = (int)strlen(ents[i].owner);        if (w > w_owner) w_owner = w;
        w = (int)strlen(ents[i].group);        if (w > w_group) w_group = w;
        w = num_width(ents[i].size);           if (w > w_size)  w_size = w;
    }

    // rebuild the buffer as a listing
    for (int i = 0; i < b->nlines; ++i) free(b->lines[i]);
    b->nlines = 0;
    buffer_clear_undo(b);
    buffer_ensure_capacity(b, n + 2);

    char header[PATH_MAX + 8];
    snprintf(header, sizeof(header), "%s:", real);
    b->lines[b->nlines++] = xstrdup(header);
    char total[64];
    snprintf(total, sizeof(total), "total %lld", total_blocks);
    b->lines[b->nlines++] = xstrdup(total);

    for (int i = 0; i < n; ++i) {
        DiredEnt *e = &ents[i];
        char line[PATH_MAX * 2 + 256];
        snprintf(line, sizeof(line), "%s %*ld %-*s %-*s %*lld %s %s%s%s",
                 e->perms,
                 w_nlink, e->nlink,
                 w_owner, e->owner,
                 w_group, e->group,
                 w_size, e->size,
                 e->mtime,
                 e->name,
                 e->link_target ? " -> " : "",
                 e->link_target ? e->link_target : "");
        b->lines[b->nlines++] = xstrdup(line);
        free(e->name);
        free(e->link_target);
    }
    free(ents);

    free(b->filename);
    b->filename = xstrdup(real);
    b->modified = 0;
    b->readonly = 1;
    b->is_dired = 1;
    return 0;
}

int buffer_save_file(Buffer *b, const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    for (int i = 0; i < b->nlines; ++i) {
        fputs(b->lines[i], f);
        fputc('\n', f);
    }
    if (fclose(f) != 0) return -1;
    free(b->filename);
    b->filename = xstrdup(path);
    b->modified = 0;
    return 0;
}

// File completion implementation
FileCompletion *file_completion_new(void) {
    FileCompletion *fc = xmalloc(sizeof(FileCompletion));
    memset(fc, 0, sizeof(FileCompletion));
    fc->capacity = 16;
    fc->matches = xmalloc(fc->capacity * sizeof(char*));
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

static void completion_add(FileCompletion *fc, const char *s) {
    if (fc->count >= fc->capacity) {
        fc->capacity *= 2;
        fc->matches = xrealloc(fc->matches, fc->capacity * sizeof(char*));
    }
    fc->matches[fc->count++] = xstrdup(s);
}

int file_completion_find_matches(FileCompletion *fc, const char *pattern) {
    // Clear existing matches
    for (int i = 0; i < fc->count; ++i) {
        free(fc->matches[i]);
    }
    fc->count = 0;
    fc->selected = 0;

    if (!pattern) pattern = "";

    // expand a leading "~/" so completion works inside the home directory
    char expanded[1024];
    if (pattern[0] == '~' && (pattern[1] == '/' || pattern[1] == '\0')) {
        const char *home = getenv("HOME");
        if (home && *home) {
            snprintf(expanded, sizeof(expanded), "%s%s", home, pattern + 1);
            pattern = expanded;
        }
    }

    char dir[512];
    char file_prefix[512];
    split_path(pattern, dir, file_prefix, sizeof(dir), sizeof(file_prefix));

    DIR *d = opendir(dir[0] ? dir : "/");
    if (!d) {
        return 0;
    }

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        // Skip hidden files unless pattern starts with '.'
        if (entry->d_name[0] == '.' && file_prefix[0] != '.') {
            continue;
        }

        if (starts_with(entry->d_name, file_prefix)) {
            char full_path[1024];
            if (strcmp(dir, ".") == 0) {
                snprintf(full_path, sizeof(full_path), "%s", entry->d_name);
            } else {
                snprintf(full_path, sizeof(full_path), "%s/%s", dir, entry->d_name);
            }

            // Check if it's a directory and add trailing slash
            struct stat st;
            if (stat(full_path, &st) == 0 && S_ISDIR(st.st_mode)) {
                size_t len = strlen(full_path);
                if (len < sizeof(full_path) - 1) {
                    full_path[len] = '/';
                    full_path[len + 1] = '\0';
                }
            }
            completion_add(fc, full_path);
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

void buffer_set_readonly(Buffer *b, int readonly) {
    if (b) {
        b->readonly = readonly ? 1 : 0;
    }
}

int buffer_is_readonly(Buffer *b) {
    return b ? b->readonly : 0;
}
