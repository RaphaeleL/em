#include <stdlib.h>
#define SHL_IMPLEMENTATION
#define SHL_STRIP_PREFIX
#include "./build.h"

Cmd cmd = {0};
Timer timer = {0};

int main() {
    auto_rebuild("build.c");
    timer_start(&timer);

    char *source_files[][2] = {
        { "emacs.c", "out/emacs.o" },
        { "buffer.c", "out/buffer.o" },
        { "display.c", "out/display.o" },
        { "input.c", "out/input.o" },
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
    push(&cmd, "-o", "out/bin/em");
    if (!run(&cmd)) return EXIT_FAILURE;

    double elapsed_ms = timer_elapsed(&timer);
    timer_reset(&timer);
    info("Finished in %.3f seconds.\n", elapsed_ms);

    return EXIT_SUCCESS;
}
