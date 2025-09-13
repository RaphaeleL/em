/* 
 * emacs.c
 *
 * A reimplementation of Emacs in C using ncurses.
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

#include "includes/buffer.h"
#include "includes/display.h"
#include "includes/input.h"

int main(int argc, char **argv) {
    initscr();
    raw();
    noecho();
    keypad(stdscr, TRUE);
    start_color();
    use_default_colors();

    EditorState E;
    memset(&E, 0, sizeof(E));
    E.buf = buffer_new();
    E.cx = E.cy = 0;
    E.row_offset = 0;
    E.col_offset = 0;
    E.popup_visible = 0;
    E.completion = NULL;
    editor_update_screen_size(&E);

    if (argc >= 2) {
        if (buffer_load_file(E.buf, argv[1]) != 0) {
            // create new buffer but set filename
            free(E.buf->filename);
            E.buf->filename = strdup(argv[1]);
            editor_message(&E, "New file: %s", argv[1]);
        } else {
            editor_message(&E, "Opened %s", argv[1]);
        }
    } else {
        editor_message(&E, "Welcome!");
    }

    while (1) {
        editor_draw(&E, NULL);
        editor_process_key(&E);
    }

    // cleanup (never reached!)
    endwin();
    buffer_free(E.buf);
    return 0;
}
