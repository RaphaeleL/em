#ifndef BUFFER_H
#define BUFFER_H

#include <stddef.h>

typedef struct UndoState {
    char **lines;
    int nlines;
    int cx, cy;
    struct UndoState *next;
} UndoState;

typedef struct {
    char **lines;    // array of null-terminated C strings
    int nlines;
    int capacity;
    int modified;
    int readonly;    // read-only flag
    int is_dired;    // buffer shows a directory listing
    char *filename;
    UndoState *undo_stack;
    int undo_depth;
} Buffer;

// File completion structures
typedef struct {
    char **matches;     // array of matching filenames
    int count;          // number of matches
    int capacity;       // allocated capacity
    int selected;       // currently selected match index
} FileCompletion;

Buffer *buffer_new(void);
void buffer_free(Buffer *b);
void buffer_ensure_capacity(Buffer *b, int newcap);
void buffer_insert_line(Buffer *b, int idx, const char *s);
void buffer_delete_line(Buffer *b, int idx);
int buffer_load_file(Buffer *b, const char *path);
int buffer_load_dir(Buffer *b, const char *path);
int buffer_save_file(Buffer *b, const char *path);
void buffer_set_readonly(Buffer *b, int readonly);
int buffer_is_readonly(Buffer *b);

// Undo: push a snapshot before a modification; undo restores the last one.
void buffer_push_undo(Buffer *b, int cx, int cy);
int buffer_undo(Buffer *b, int *cx, int *cy);
void buffer_clear_undo(Buffer *b);

FileCompletion *file_completion_new(void);
void file_completion_free(FileCompletion *fc);
int file_completion_find_matches(FileCompletion *fc, const char *pattern);
char *file_completion_get_selected(FileCompletion *fc);
void file_completion_next(FileCompletion *fc);
void file_completion_prev(FileCompletion *fc);

// Allocation helpers that abort cleanly instead of corrupting memory on OOM.
void *xmalloc(size_t n);
void *xrealloc(void *p, size_t n);
char *xstrdup(const char *s);

#endif // BUFFER_H
