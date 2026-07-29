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
    set_escdelay(50); // make lone ESC presses resolve quickly
    start_color();
    use_default_colors();

    EditorState E;
    memset(&E, 0, sizeof(E));
    E.buf = buffer_new();
    editor_update_screen_size(&E);

    if (argc >= 2) {
        editor_visit_path(&E, argv[1]);
    } else {
        editor_message(&E, "Welcome! M-x help for key bindings, C-x C-c to quit.");
    }

    while (1) {
        editor_process_key(&E);
        editor_draw(&E, NULL);
    }

    // cleanup (never reached!)
    endwin();
    buffer_free(E.buf);
    return 0;
}
