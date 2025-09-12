#ifndef BUFFER_H
#define BUFFER_H

typedef struct {
    char **lines;    // array of null-terminated C strings
    int nlines;
    int capacity;
    int modified;
    char *filename;
} Buffer;

Buffer *buffer_new(void);
void buffer_free(Buffer *b);
void buffer_ensure_capacity(Buffer *b, int newcap);
void buffer_insert_line(Buffer *b, int idx, const char *s);
void buffer_delete_line(Buffer *b, int idx);
int buffer_load_file(Buffer *b, const char *path);
int buffer_save_file(Buffer *b, const char *path);

#endif // BUFFER_H
