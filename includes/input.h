#ifndef INPUT_H
#define INPUT_H

#include <ctype.h>
#include "display.h"

void editor_insert_char(EditorState *E, int c);
void editor_backspace(EditorState *E);
void editor_enter(EditorState *E);
int editor_minibuffer_getline(EditorState *E, const char *prompt, char *out, size_t outcap);
void editor_process_key(EditorState *E);

#endif // INPUT_H
