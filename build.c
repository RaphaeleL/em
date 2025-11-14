#include <stdlib.h>
#define SHL_IMPLEMENTATION
#define SHL_STRIP_PREFIX
#include "./build.h"

Cmd cmd = {0};


// ~/Projects/em> ./build 
// [INFO] cc   -g -O2 -Wall -Wextra -std=c99 -c emacs.c -o emacs.o 
// [INFO] cc   -g -O2 -Wall -Wextra -std=c99 -c buffer.c -o buffer.o 
// [INFO] cc   -g -O2 -Wall -Wextra -std=c99 -c display.c -o display.o 
// [INFO] cc   -g -O2 -Wall -Wextra -std=c99 -c input.c -o input.o 
// [INFO] cc   -g -O2 -Wall -Wextra -std=c99 -lncurses emacs.o buffer.o display.o input.o -o em 

int main() {
    auto_rebuild("build.c");

    char *source_files[][2] = {
        { "emacs.c", "emacs.o" },
        { "buffer.c", "buffer.o" },
        { "display.c", "display.o" },
        { "input.c", "input.o" },
    };

    for (size_t i = 0; i < ARRAY_LEN(source_files); i++) {
        push(&cmd, "cc", "-c", source_files[i][0]);
        push(&cmd, "-g", "-O2", "-Wall", "-Wextra", "-std=c99");
        push(&cmd, "-o", source_files[i][1]);
        if (!run(&cmd)) return EXIT_FAILURE;
    }

    push(&cmd, "cc", "-g", "-O2", "-Wall", "-Wextra");
    push(&cmd, "-std=c99", "-lncurses");
    for (size_t i = 0; i < ARRAY_LEN(source_files); i++) {
        push(&cmd, source_files[i][1]);
    }
    push(&cmd, "-o", "em");
    if (!run_always(&cmd)) return EXIT_FAILURE;

    return EXIT_SUCCESS;
}
