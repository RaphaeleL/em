#ifndef INPUT_H
#define INPUT_H

#include <ctype.h>
#include "display.h"

void editor_insert_char(EditorState *E, int c);
void editor_backspace(EditorState *E);
void editor_enter(EditorState *E);
int editor_minibuffer_getline(EditorState *E, const char *prompt, char *out, size_t outcap);
int editor_minibuffer_getline_with_completion(EditorState *E, const char *prompt, char *out, size_t outcap);
void editor_process_key(EditorState *E);

// Command system
void editor_command_mode(EditorState *E);
void editor_execute_command(EditorState *E, const char *command);
void editor_show_help(EditorState *E);

#endif // INPUT_H
