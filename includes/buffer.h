#ifndef BUFFER_H
#define BUFFER_H

typedef struct {
    char **lines;    // array of null-terminated C strings
    int nlines;
    int capacity;
    int modified;
    int readonly;    // read-only flag
    char *filename;
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
int buffer_save_file(Buffer *b, const char *path);
void buffer_set_readonly(Buffer *b, int readonly);
int buffer_is_readonly(Buffer *b);

FileCompletion *file_completion_new(void);
void file_completion_free(FileCompletion *fc);
int file_completion_find_matches(FileCompletion *fc, const char *pattern);
char *file_completion_get_selected(FileCompletion *fc);
void file_completion_next(FileCompletion *fc);
void file_completion_prev(FileCompletion *fc);

#endif // BUFFER_H
