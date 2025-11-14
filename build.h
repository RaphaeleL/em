/* build.h - v0.0.1 - https://github.com/RaphaeleL/build.h
   ============================================================================
    File: build.h
    Description: Quality-of-life utilities and abstractions for C development.

    This header provides a collection of macros, inline utilities, build-time
    helpers and more, intended to simplify common C programming patterns,
    improve code clarity, and enhance portability across compilers and
    platforms.

    The Idea could be considered as a mix between nothings/stb and tsoding/nob.h

    ----------------------------------------------------------------------------
    Created : 30 Sep 2025
    Changed : 14 Nov 2025
    Author  : Raphaele Salvatore Licciardo
    License : MIT (see LICENSE for details)
    Version : 0.0.1
    ----------------------------------------------------------------------------

    Quick Example: Auto Rebuild the Build System

      // build.c
      #define SHL_IMPLEMENTATION
      #define SHL_STRIP_PREFIX
      #include "./build.h"

      int main() {
          auto_rebuild("build.c");

          Cmd build = default_c_build("demo.c", "demo");
          if (!run(&build)) {  // auto-releases on success or failure
              return 1;
          }
          // -> run `cc -o demo demo.c` on change of source file

          Cmd calc = default_c_build("calc.c", NULL);
          push(&calc, "-Wall", "-Wextra");
          if (!shl_run_always(&calc)) {  // auto-releases on success or failure
              return 1;
          }
          // -> run `cc -Wall -Wextra calc.c -o calc` always

          return 0;
      }

    Further Example: Build System, Arg Parser, Helpers, Hashmap, Logger

      // demo.c
      #define SHL_IMPLEMENTATION
      #define SHL_STRIP_PREFIX
      #include "./build.h"

      int main(int argc, char** argv) {
          auto_rebuild_plus("demo.c", "build.h");

          const char* progr = shift(argc, argv);
          const char* param = shift(argc, argv);

          UNUSED(progr);

          if (strcmp(param, "hashmap")) {
              info("Hashmap Demo selected\n");
              HashMap* hm = hm_create();
              hm_put(hm, (void*)"prename", (void*)"john");
              hm_put(hm, (void*)"name", (void*)"doe");
              hm_put(hm, (void*)"age", (void*)30);
              hm_remove(hm, (void *)"prename");
              hm_release(hm);
          } else {
              info("No Demo selected\n");
          }
          return 0;
      }

    ----------------------------------------------------------------------------
    Copyright (c) 2025 Raphaele Salvatore Licciardo

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
   ============================================================================ */

#pragma once

#ifndef SHL_BUILD_H
#define SHL_BUILD_H

#ifdef __cplusplus
    extern "C" {
#endif // __cplusplus

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifndef SHL_ASSERT
    #include <assert.h>
    #define SHL_ASSERT assert
#endif /* SHL_ASSERT */

#if (defined(__APPLE__) && defined(__MACH__)) || defined(__linux__)
    #include <pthread.h>
    #include <unistd.h>
    #include <dirent.h>
    #ifndef _POSIX_C_SOURCE
        #define _POSIX_C_SOURCE 199309L
    #endif
    #include <time.h>
#elif defined(_WIN32) || defined(_WIN64)
    #include <windows.h>
    #include <io.h>
    #include <direct.h>
#else
    #error Unsupported platform
#endif

//////////////////////////////////////////////////
/// LOGGER ///////////////////////////////////////
//////////////////////////////////////////////////

// Log levels
typedef enum {
    SHL_LOG_DEBUG = 0,   // like an extended log
    SHL_LOG_INFO,        //
    SHL_LOG_CMD,         // an executed command
    SHL_LOG_HINT,        // special hint, not something wrong
    SHL_LOG_WARN,        // something before error
    SHL_LOG_ERROR,       // well, the ship is sinking
    SHL_LOG_CRITICAL,    // fucked up as hard as possible
    SHL_LOG_NONE
} shl_log_level_t;

// Initialize logger
void shl_init_logger(shl_log_level_t level, bool color, bool time);

// Forward declaration for logging function
void shl_log(shl_log_level_t level, const char *fmt, ...);

#define shl_debug(fmt, ...)    shl_log(SHL_LOG_DEBUG, fmt, ##__VA_ARGS__)
#define shl_info(fmt, ...)     shl_log(SHL_LOG_INFO, fmt, ##__VA_ARGS__)
#define shl_cmd(fmt, ...)      shl_log(SHL_LOG_CMD, fmt, ##__VA_ARGS__)
#define shl_hint(fmt, ...)     shl_log(SHL_LOG_HINT, fmt, ##__VA_ARGS__)
#define shl_warn(fmt, ...)     shl_log(SHL_LOG_WARN, fmt, ##__VA_ARGS__)
#define shl_error(fmt, ...)    shl_log(SHL_LOG_ERROR, fmt, ##__VA_ARGS__)
#define shl_critical(fmt, ...) shl_log(SHL_LOG_CRITICAL, fmt, ##__VA_ARGS__)

//////////////////////////////////////////////////
/// CLI_PARSER ///////////////////////////////////
//////////////////////////////////////////////////

#define SHL_ARG_MAX 128

typedef struct {
    const char *long_name;   // "--foo"
    char short_name;         // 'f'
    const char *default_val; // default value as string
    const char *help_msg;    // help message
    const char *value;       // actual value from argv
} shl_arg_t;

typedef struct {
    shl_arg_t args[SHL_ARG_MAX];
    int count;
} shl_argparser_t;

extern shl_argparser_t shl_parser;

// initialize the cli argument parser
void shl_init_argparser(int argc, char *argv[]);
// create an cli argument
void shl_add_argument(const char *long_name, const char *default_val, const char *help_msg);
// check if an argument is set and return the struct with the data
shl_arg_t *shl_get_argument(const char *long_name);

// pops an element from the beginning of a sized array (tsoding/nob.h)
#define shl_shift(size, elements) (SHL_ASSERT((size) > 0), (size)--, *(elements)++)

//////////////////////////////////////////////////
/// NO_BUILD /////////////////////////////////////
//////////////////////////////////////////////////

#define MAX_TASKS 32

typedef struct {
    const char **data;
    size_t len;
    size_t cap;
} SHL_Cmd;

typedef struct {
    SHL_Cmd config;
    bool success;
} SHL_CmdTask;

// get the default compiler flags depending on the plattform
static inline char *shl_default_compiler_flags(void);
// Build a default build command (cc source -o output)
SHL_Cmd shl_default_c_build(const char *source, const char *output);

// Runs a BuildConfig based on the Timestamp
bool shl_run(SHL_Cmd *config);
// Always runs a BuildConfig
bool shl_run_always(SHL_Cmd* config);
// Auto Rebuild a Source File depending on the Timestamp
void shl_auto_rebuild(const char *src);
// Auto Rebuild a Source File and it's deps depending on the Timestamp
void shl_auto_rebuild_plus_impl(const char *src, ...);

// Macro to automatically append NULL to variadic args
#define shl_auto_rebuild_plus(src, ...) shl_auto_rebuild_plus_impl(src, __VA_ARGS__, NULL)
// get the filename without it's extension, can be used to auto gen the output
// of a source file. TODO: Should be in @FILE_OPS, not in @NO_BUILD
char *shl_get_filename_no_ext(const char *path);

//////////////////////////////////////////////////
/// FILE_OPS /////////////////////////////////////
//////////////////////////////////////////////////

typedef struct {
    char **data;
    size_t len;
    size_t cap;
} SHL_String;

bool shl_mkdir(const char *path);
bool shl_mkdir_if_not_exists(const char *path);
bool shl_copy_file(const char *src_path, const char *dst_path);
bool shl_copy_dir_rec(const char *src_path, const char *dst_path);
bool shl_read_dir(const char *parent, const char *children);
bool shl_read_file(const char *path, SHL_String* content);
bool shl_write_file(const char *path, const void *data, size_t size);
const char *shl_get_file_type(const char *path);
bool shl_delete_file(const char *path);
void shl_release_string(SHL_String* content);

//////////////////////////////////////////////////
/// DYN_ARRAY ////////////////////////////////////
//////////////////////////////////////////////////

#define SHL_INIT_CAP 8

// Reserve space for at least `n` elements
#define shl_grow(vec, n)                                                                                     \
    do {                                                                                                     \
        if ((n) > (vec)->cap) {                                                                              \
            size_t newcap = (vec)->cap ? (vec)->cap : SHL_INIT_CAP;                                          \
            while (newcap < (n)) newcap *= 2;                                                                \
            if ((vec)->cap == 0) {                                                                           \
                shl_log(SHL_LOG_DEBUG, "Dynamic array inits memory on %d.\n", newcap);                       \
            } else {                                                                                         \
                shl_log(SHL_LOG_DEBUG, "Dynamic array needs more memory (%d -> %d)!\n", (vec)->cap, newcap); \
            }                                                                                                \
            void *tmp = realloc((vec)->data, newcap * sizeof(*(vec)->data));                                 \
            if (!tmp) {                                                                                      \
                shl_log(SHL_LOG_ERROR, "Dynamic array out of memory (need %zu elements)\n", n);              \
                abort();                                                                                     \
            }                                                                                                \
            (vec)->data = tmp;                                                                               \
            (vec)->cap = newcap;                                                                             \
        }                                                                                                    \
    } while (0)

#define shl_shrink(vec)                                                                                        \
    do {                                                                                                       \
        if ((vec)->len < (vec)->cap / 2 && (vec)->cap > SHL_INIT_CAP) {                                        \
            size_t newcap = (vec)->cap / 2;                                                                    \
            shl_log(SHL_LOG_DEBUG, "Dynamic array can release some memory (%d -> %d)!\n", (vec)->cap, newcap); \
            void *tmp = realloc((vec)->data, newcap * sizeof(*(vec)->data));                                   \
            if (tmp) {                                                                                         \
                (vec)->data = tmp;                                                                             \
                (vec)->cap = newcap;                                                                           \
            }                                                                                                  \
        }                                                                                                      \
    } while (0)

// Push a single item (internal implementation)
#define shl_push_impl(vec, val)            \
    do {                                   \
        shl_grow((vec), (vec)->len+1);     \
        (vec)->data[(vec)->len++] = (val); \
    } while (0)

// Variadic push: push(&vec, val) or push(&vec, a, b, c, ...) - truly dynamic, no limits
// Uses compound literal array trick - works with any number of arguments
// Note: Uses typeof (GCC/Clang extension) for type inference
#define shl_push(vec, ...) \
    do { \
        typeof(*vec) *__vec = (vec); \
        typeof(__vec->data[0]) __temp[] = {__VA_ARGS__}; \
        size_t __count = sizeof(__temp) / sizeof(__temp[0]); \
        for (size_t __i = 0; __i < __count; __i++) { \
            shl_push_impl(__vec, __temp[__i]); \
        } \
    } while (0)


// Remove the last element
#define shl_drop(vec)                                              \
    do {                                                           \
        if ((vec)->len == 0) {                                     \
            shl_log(SHL_LOG_ERROR, "shl_drop() on empty array\n"); \
            abort();                                               \
        }                                                          \
        --(vec)->len;                                              \
        shl_shrink(vec);                                           \
    } while (0)

// Remove element at index n (shift elements down)
#define shl_dropn(vec, n)                                                \
    do {                                                                 \
        size_t __idx = (n);                                              \
        if (__idx >= (vec)->len) {                                       \
            shl_log(SHL_LOG_ERROR, "shl_dropn(): index out of range\n"); \
            abort();                                                     \
        }                                                                \
        memmove((vec)->data + __idx,                                     \
                (vec)->data + __idx + 1,                                 \
                ((vec)->len - __idx - 1) * sizeof(*(vec)->data));        \
        --(vec)->len;                                                    \
        shl_shrink(vec);                                                 \
    } while (0)

// Resize array to exactly n elements (uninitialized if grown)
#define shl_resize(vec, n)    \
    do {                      \
        shl_grow((vec), (n)); \
        (vec)->len = (n);     \
    } while (0)

// Free the buffer
#define shl_release(vec)             \
    do {                             \
        free((vec)->data);           \
        (vec)->data = NULL;          \
        (vec)->len = (vec)->cap = 0; \
    } while (0)

// Get last element (asserts non-empty)
#define shl_back(vec) \
    ((vec)->len > 0 ? (vec)->data[(vec)->len-1] : \
     (fprintf(stderr, "[ERROR] shl_back() on empty array\n"), abort(), (vec)->data[0]))

// Swap element i with last element (without removing)
#define shl_swap(vec, i)                                          \
    do {                                                          \
        size_t __idx = (i);                                       \
        if (__idx >= (vec)->len) {                                \
            shl_log(SHL_LOG_ERROR, "shl_swap(): out of range\n"); \
            abort();                                              \
        }                                                         \
        typeof((vec)->data[0]) __tmp = (vec)->data[__idx];        \
        (vec)->data[__idx] = (vec)->data[(vec)->len - 1];         \
        (vec)->data[(vec)->len - 1] = __tmp;                      \
    } while (0)

// Struct wrapper
#define shl_list(T) \
    struct { T *data; size_t len, cap; }

//////////////////////////////////////////////////
/// HASHMAP //////////////////////////////////////
//////////////////////////////////////////////////

typedef enum {
    SHL_HM_EMPTY = 0,
    SHL_HM_USED,
    SHL_HM_DELETED
} shl_hm_entry_state_t;

typedef struct {
    void *key;
    void *value;
    size_t key_size;
    size_t value_size;
    shl_hm_entry_state_t state;
} SHL_HashMapEntry;

typedef struct {
    SHL_HashMapEntry *buckets;
    size_t capacity;
    size_t size;
} SHL_HashMap;

// Create an empty hashmap
SHL_HashMap *shl_hm_create();
// put a key/value pair into the hashmap
void shl_hm_put(SHL_HashMap *hm, void *key, void *value);
// get a value, based on the key
void *shl_hm_get(SHL_HashMap *hm, void *key);
// check if a hashmap contains a key
bool shl_hm_contains(SHL_HashMap *hm, void *key);
// remove a key/value pair, based on the key
bool shl_hm_remove(SHL_HashMap *hm, void *key);
// delete all the entries of an hashmap
void shl_hm_clear(SHL_HashMap* hm);
// free the hashmap memory
void shl_hm_release(SHL_HashMap* hm);
// get the size of a hashmap
size_t shl_hm_size(SHL_HashMap* hm);
// empty a hashmap
bool shl_hm_empty(SHL_HashMap* hm);

//////////////////////////////////////////////////
/// HELPER ///////////////////////////////////////
//////////////////////////////////////////////////

#define SHL_UNUSED(value) (void)(value)
#define SHL_TODO(message) do { fprintf(stderr, "%s:%d: TODO: %s\n", __FILE__, __LINE__, message); abort(); } while(0)
#define SHL_UNREACHABLE(message) do { fprintf(stderr, "%s:%d: UNREACHABLE: %s\n", __FILE__, __LINE__, message); abort(); } while(0)

#define SHL_ARRAY_LEN(array) (sizeof(array)/sizeof(array[0]))
#define SHL_ARRAY_GET(array, index) \
    (SHL_ASSERT((size_t)(index) < SHL_ARRAY_LEN(array)), (array)[(size_t)(index)])

//////////////////////////////////////////////////
/// UNITTEST /////////////////////////////////////
//////////////////////////////////////////////////

typedef struct {
    void (*func)(void);
    const char *name;
    const char *file;
    int line;
} shl_test_t;

void shl_test_register(const char *name, const char *file, int line, void (*test_func)(void));
void shl_test_fail(void);
int shl_test_run_all(void);
void shl_test_print_summary(void);

// Internal failure message storage
extern char shl_test_failure_msg[];

// Test macros
#define SHL_TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            snprintf(shl_test_failure_msg, sizeof(shl_test_failure_msg), "%s:%d: %s", __FILE__, __LINE__, message); \
            shl_test_fail(); \
            return; \
        } \
    } while(0)

#define SHL_TEST_EQ(a, b, message) SHL_TEST_ASSERT((a) == (b), message)
#define SHL_TEST_NEQ(a, b, message) SHL_TEST_ASSERT((a) != (b), message)
#define SHL_TEST_STREQ(a, b, message) SHL_TEST_ASSERT(strcmp((a), (b)) == 0, message)
#define SHL_TEST_STRNEQ(a, b, message) SHL_TEST_ASSERT(strcmp((a), (b)) != 0, message)
#define SHL_TEST_TRUTHY(value, message) SHL_TEST_ASSERT(value, message)
#define SHL_TEST_FALSY(value, message) SHL_TEST_ASSERT(!(value), message)

#define SHL_TEST(name) \
    static void shl_test_##name(void); \
    __attribute__((constructor)) static void shl_test_register_##name(void) { \
        shl_test_register(#name, __FILE__, __LINE__, shl_test_##name); \
    } \
    static void shl_test_##name(void)

//////////////////////////////////////////////////
/// TIMER ////////////////////////////////////////
//////////////////////////////////////////////////

// High-resolution timer structure
typedef struct {
#if defined(_WIN32) || defined(_WIN64)
    LARGE_INTEGER start;
    LARGE_INTEGER frequency;
#else
    struct timespec start;
#endif
} SHL_Timer;

// Start a timer
void shl_timer_start(SHL_Timer *timer);

// Get elapsed time in seconds (as double)
double shl_timer_elapsed(SHL_Timer *timer);

// Get elapsed time in milliseconds (as double)
double shl_timer_elapsed_ms(SHL_Timer *timer);

// Get elapsed time in microseconds (as double)
double shl_timer_elapsed_us(SHL_Timer *timer);

// Get elapsed time in nanoseconds (as uint64_t)
uint64_t shl_timer_elapsed_ns(SHL_Timer *timer);

// Reset timer (restart from now)
void shl_timer_reset(SHL_Timer *timer);

//////////////////////////////////////////////////
/// SHL_IMPLEMENATION ////////////////////////////
//////////////////////////////////////////////////

#ifdef SHL_IMPLEMENTATION

    //////////////////////////////////////////////////
    /// LOGGER ///////////////////////////////////////
    //////////////////////////////////////////////////

    #define SHL_COLOR_RESET     "\x1b[0m"
    #define SHL_COLOR_ERROR     "\x1b[31m" // red
    #define SHL_COLOR_INFO      "\x1b[32m" // green
    #define SHL_COLOR_WARN      "\x1b[33m" // yellow
    #define SHL_COLOR_HINT      "\x1b[34m" // blue
    #define SHL_COLOR_CRITICAL  "\x1b[35m" // purple
    #define SHL_COLOR_CMD       "\x1b[36m" // yellow
    #define SHL_COLOR_DEBUG     "\x1b[90m" // gray

    static shl_log_level_t shl_logger_min_level = SHL_LOG_INFO;
    static bool shl_logger_color = false;
    static bool shl_logger_time = !false;

    void shl_init_logger(shl_log_level_t level, bool color, bool time) {
        shl_logger_min_level = level;
        shl_logger_color = color;
        shl_logger_time = time;
    }

    static const char *shl_level_to_str(shl_log_level_t level) {
        switch (level) {
        case SHL_LOG_DEBUG:    return "DEBUG";
        case SHL_LOG_INFO:     return "INFO";
        case SHL_LOG_CMD:      return "CMD";
        case SHL_LOG_HINT:     return "HINT";
        case SHL_LOG_WARN:     return "WARN";
        case SHL_LOG_ERROR:    return "ERROR";
        case SHL_LOG_CRITICAL: return "CRITICAL";
        default:               return "UNK";
        }
    }

    static const char *shl_level_to_color(shl_log_level_t level) {
        switch (level) {
        case SHL_LOG_DEBUG:    return SHL_COLOR_DEBUG;
        case SHL_LOG_INFO:     return SHL_COLOR_INFO;
        case SHL_LOG_CMD:      return SHL_COLOR_CMD;
        case SHL_LOG_HINT:     return SHL_COLOR_HINT;
        case SHL_LOG_WARN:     return SHL_COLOR_WARN;
        case SHL_LOG_ERROR:    return SHL_COLOR_ERROR;
        case SHL_LOG_CRITICAL: return SHL_COLOR_CRITICAL;
        default:               return SHL_COLOR_RESET;
        }
    }

    void shl_log(shl_log_level_t level, const char *fmt, ...) {
        if (level < shl_logger_min_level || level >= SHL_LOG_NONE) return;

        const char *level_str = shl_level_to_str(level);

        const char *level_color = "";
        if (shl_logger_color) {
            level_color = shl_level_to_color(level);
        }

        char time_buf[32] = {0};
        if (shl_logger_time) {
            time_t t = time(NULL);
            struct tm *lt = localtime(&t);
            strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", lt);
        }

        if (shl_logger_time) {
            fprintf(stderr, "%s[%s]%s %s >>> ", level_color, level_str, SHL_COLOR_RESET, time_buf);
        } else {
            fprintf(stderr, "%s[%s]%s ", level_color, level_str, SHL_COLOR_RESET);
        }

        if (level == SHL_LOG_ERROR || level == SHL_LOG_CRITICAL) {
            fprintf(stderr, "\t\n");
            fprintf(stderr, "\t\n");
            fprintf(stderr, "\t              |    |    |                 \n");
            fprintf(stderr, "\t             )_)  )_)  )_)                %s: Leaving the Ship\n", level_str);
            fprintf(stderr, "\t            )___))___))___)               > ");
            va_list args;
            va_start(args, fmt);
            vfprintf(stderr, fmt, args);
            va_end(args);
            fprintf(stderr, "\t           )____)____)_____)              \n");
            fprintf(stderr, "\t         _____|____|____|_____            \n");
            fprintf(stderr, "\t---------\\                   /---------  \n");
            fprintf(stderr, "\t  ^^^^^ ^^^^^^^^^^^^^^^^^^^^^             \n");
            fprintf(stderr, "\t    ^^^^      ^^^^     ^^^    ^^          \n");
            fprintf(stderr, "\t         ^^^^      ^^^                    \n");
            fprintf(stderr, "\t\n");
        } else {
            va_list args;
            va_start(args, fmt);
            vfprintf(stderr, fmt, args);
            va_end(args);
        }

        if (level == SHL_LOG_ERROR) {
            fflush(NULL);
            exit(EXIT_FAILURE);
        } else if (level == SHL_LOG_CRITICAL) {
            fflush(NULL);
            abort();
        }
    }

    //////////////////////////////////////////////////1
    /// CLI_PARSER ///////////////////////////////////1
    //////////////////////////////////////////////////

    void shl_init_argparser(int argc, char *argv[]) {
        shl_add_argument("--help", NULL, "Show this help message"); // no value expected

        for (int i = 1; i < argc; i++) {
            for (int j = 0; j < shl_parser.count; j++) {
                shl_arg_t *arg = &shl_parser.args[j];

                // Long option match
                if (strcmp(argv[i], arg->long_name) == 0) {
                    if (strcmp(arg->long_name, "--help") == 0) {
                        arg->value = "1"; // flag is set
                    } else if (i + 1 < argc && argv[i + 1][0] != '-') {
                        arg->value = argv[i + 1];
                        i++;
                    } else {
                        arg->value = "1"; // flag is set (no value provided)
                    }
                }
                // Short option match
                else if (argv[i][0] == '-' && argv[i][1] == arg->short_name) {
                    if (arg->short_name == 'h') {
                        arg->value = "1"; // flag is set
                    } else if (i + 1 < argc && argv[i + 1][0] != '-') {
                        arg->value = argv[i + 1];
                        i++;
                    } else {
                        arg->value = "1"; // flag is set (no value provided)
                    }
                }
            }
        }

        // Show help if requested
        shl_arg_t *help = shl_get_argument("--help");
        if (help && help->value) {
            printf("Usage:\n");
            for (int i = 0; i < shl_parser.count; i++) {
                shl_arg_t *arg = &shl_parser.args[i];
                printf("  %s, -%c: %s (default: %s)\n",
                    arg->long_name,
                    arg->short_name,
                    arg->help_msg ? arg->help_msg : "",
                    arg->default_val ? arg->default_val : "none");
            }
            exit(0);
        }
    }

    shl_argparser_t shl_parser = { .count = 0 };

    void shl_add_argument(const char *long_name, const char *default_val, const char *help_msg) {
        if (shl_parser.count >= SHL_ARG_MAX) {
            shl_log(SHL_LOG_ERROR, "Maximum number of arguments reached\n");
            return;
        }
        shl_arg_t *arg = &shl_parser.args[shl_parser.count++];
        arg->long_name = long_name;
        arg->short_name = long_name[2]; // take first letter after '--'
        arg->default_val = default_val;
        arg->help_msg = help_msg;
        arg->value = default_val; // initial value
    }

    shl_arg_t *shl_get_argument(const char *long_name) {
        for (int i = 0; i < shl_parser.count; i++) {
            if (strcmp(shl_parser.args[i].long_name, long_name) == 0)
            return &shl_parser.args[i];
        }
        return NULL;
    }

    int shl_arg_as_int(shl_arg_t *arg) {
        if (!arg || !arg->value) return 0;
        return atoi(arg->value);
    }

    const char *shl_arg_as_string(shl_arg_t *arg) {
        if (!arg) return "";
        return arg->value ? arg->value : "";
    }

    //////////////////////////////////////////////////
    /// NO_BUILD /////////////////////////////////////
    //////////////////////////////////////////////////

    static void shl_ensure_dir_for_file(const char* filepath) {
        char dir[1024];
        strncpy(dir, filepath, sizeof(dir));
        dir[sizeof(dir)-1] = '\0';

        // Find the last slash/backslash
        char *slash = strrchr(dir, '/');
#if defined(_WIN32) || defined(_WIN64)
        if (!slash) slash = strrchr(dir, '\\');
#endif
        if (slash) {
            *slash = '\0';
            shl_mkdir_if_not_exists(dir);
        }
    }

    static inline char* shl_default_compiler_flags(void) {
#if defined(_WIN32) || defined(_WIN64)
        return "";
#elif defined(__APPLE__) && defined(__MACH__)
        return "-Wall -Wextra";
#elif defined(__linux__)
        return "-Wall -Wextra";
#else
        return "";
#endif
    }

    SHL_Cmd shl_default_c_build(const char *source, const char *output) {
        SHL_Cmd cmd = {0};
        
#if defined(_WIN32) || defined(_WIN64)
        shl_push(&cmd, "gcc");
#elif defined(__APPLE__) && defined(__MACH__)
        shl_push(&cmd, "cc");
#elif defined(__linux__)
        shl_push(&cmd, "cc");
#else
        shl_push(&cmd, "cc");
#endif
        
        // Push compiler flags as separate arguments
#if !defined(_WIN32) && !defined(_WIN64)
        shl_push(&cmd, "-Wall");
        shl_push(&cmd, "-Wextra");
#endif
        
        shl_push(&cmd, source);
        shl_push(&cmd, "-o");
        
        if (output) {
            shl_push(&cmd, output);
        } else {
            char *auto_output = shl_get_filename_no_ext(source);
            if (auto_output) {
                shl_push(&cmd, auto_output);
                free(auto_output);
            }
        }
        
        return cmd;
    }

    static bool shl_is_path1_modified_after_path2(const char *path1, const char *path2) {
        struct stat stat1, stat2;

        if (stat(path1, &stat1) != 0) return false;
        if (stat(path2, &stat2) != 0) return true;

        return difftime(stat1.st_mtime, stat2.st_mtime) > 0;
    }

    char *shl_get_filename_no_ext(const char *path) {
        // Find last '/' or '\\' for Windows paths
        const char *slash = strrchr(path, '/');
        const char *backslash = strrchr(path, '\\');
        const char *base = path;

        if (slash || backslash) {
            if (slash && backslash)
            base = (slash > backslash) ? slash + 1 : backslash + 1;
            else if (slash)
            base = slash + 1;
            else
            base = backslash + 1;
        }

        // Copy base name to buffer
        char *copy = strdup(base);
        if (!copy) return NULL;

        // Remove extension if any
        char *dot = strrchr(copy, '.');
        if (dot) *dot = '\0';

        return copy; // caller must free
    }

    void shl_auto_rebuild(const char *src) {
        if (!src) return;

        struct stat src_attr, out_attr;

#if defined(_WIN32) || defined(_WIN64)
        char *out = "build_new.exe";
#else
        char *out = shl_get_filename_no_ext(src);
        if (!out) return;
#endif

        if (stat(src, &src_attr) != 0) {
            shl_log(SHL_LOG_ERROR, "No such file or directory (%s).\n", src);
#if !defined(_WIN32) && !defined(_WIN64)
            free(out);
#endif
            return;
        }

        bool need_rebuild = false;
        if (stat(out, &out_attr) != 0) {
            need_rebuild = true;
        } else if (difftime(src_attr.st_mtime, out_attr.st_mtime) > 0) {
            need_rebuild = true;
        }

        if (need_rebuild) {
            shl_debug("Rebuilding: %s -> %s\n", src, out);
#if (defined(__APPLE__) && defined(__MACH__)) || defined(__linux__)
            SHL_Cmd own_build = shl_default_c_build(src, out);
            if (!shl_run_always(&own_build)) {
                shl_release(&own_build);
                shl_log(SHL_LOG_ERROR, "Rebuild failed.\n");
#if !defined(_WIN32) && !defined(_WIN64)
                free(out);
#endif
                exit(1);
            }
            shl_release(&own_build);

            shl_debug("Restarting with updated build executable...\n");
            char *restart_argv[] = {out, NULL};
            execv(out, restart_argv);
            shl_log(SHL_LOG_ERROR, "Failed to restart build process.\n");
#if !defined(_WIN32) && !defined(_WIN64)
            free(out);
#endif
            exit(1);
#elif defined(_WIN32) || defined(_WIN64)
            SHL_Cmd own_build = shl_default_c_build(src, out);
            if (!shl_run_always(&own_build)) {
                shl_release(&own_build);
                shl_log(SHL_LOG_ERROR, "Rebuild failed.\n");
                exit(1);
            }
            shl_release(&own_build);

            shl_debug("Restarting with updated build executable...\n");
            STARTUPINFO si = { sizeof(si) };
            PROCESS_INFORMATION pi;
            if (!CreateProcess(out, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
                shl_log(SHL_LOG_ERROR, "Failed to restart build process.\n");
                exit(1);
            }
            ExitProcess(0);
#else
            #error Unsupported platform
#endif
        } else {
            shl_debug("Up to date: %s\n", out);
#if !defined(_WIN32) && !defined(_WIN64)
            free(out);
#endif
        }
    }

    // Auto-rebuild with additional dependency checking
    void shl_auto_rebuild_plus_impl(const char *src, ...) {
        if (!src) return;
        struct stat src_attr, out_attr;
#if defined(_WIN32) || defined(_WIN64)
        const char *out = "build_new.exe";
#else
        char *out = shl_get_filename_no_ext(src);
#endif
        if (stat(src, &src_attr) != 0) {
            shl_log(SHL_LOG_ERROR, "No such file or directory (%s).\n", src);
#if !defined(_WIN32) && !defined(_WIN64)
            free(out);
#endif
            return;
        }

        bool need_rebuild = false;
        if (stat(out, &out_attr) != 0) {
            need_rebuild = true;
        } else if (difftime(src_attr.st_mtime, out_attr.st_mtime) > 0) {
            need_rebuild = true;
        }

        // Check additional dependencies from variadic arguments
        if (!need_rebuild) {
            va_list args;
            va_start(args, src);
            const char *dep_file = va_arg(args, const char*);
            while (dep_file != NULL) {
                if (shl_is_path1_modified_after_path2(dep_file, out)) {
                    shl_log(SHL_LOG_DEBUG, "Dependency %s is newer than binary, rebuild needed\n", dep_file);
                    need_rebuild = true;
                    // Don't break - continue checking all dependencies for logging
                }
                dep_file = va_arg(args, const char*);
            }
            va_end(args);
        }

        if (need_rebuild) {
            shl_debug("Rebuilding: %s -> %s\n", src, out);
#if (defined(__APPLE__) && defined(__MACH__)) || defined(__linux__)
            SHL_Cmd own_build = shl_default_c_build(src, out);
            if (!shl_run_always(&own_build)) {
                shl_release(&own_build);
                shl_log(SHL_LOG_ERROR, "Rebuild failed.\n");
#if !defined(_WIN32) && !defined(_WIN64)
                free(out);
#endif
                exit(1);
            }
            shl_release(&own_build);

            shl_debug("Restarting with updated build executable...\n");
            char *restart_argv[] = {out, NULL};
            execv(out, restart_argv);
            shl_log(SHL_LOG_ERROR, "Failed to restart build process.\n");
#if !defined(_WIN32) && !defined(_WIN64)
            free(out);
#endif
            exit(1);
#elif defined(_WIN32) || defined(_WIN64)
            SHL_Cmd own_build = shl_default_c_build(src, out);
            if (!shl_run_always(&own_build)) {
                shl_release(&own_build);
                shl_log(SHL_LOG_ERROR, "Rebuild failed.\n");
                exit(1);
            }
            shl_release(&own_build);

            shl_debug("Restarting with updated build executable...\n");
            STARTUPINFO si = { sizeof(si) };
            PROCESS_INFORMATION pi;
            char cmdline[1024];
            snprintf(cmdline, sizeof(cmdline), "\"%s\"", out);
            if (!CreateProcess(NULL, cmdline, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
                shl_log(SHL_LOG_ERROR, "Failed to restart build process.\n");
                exit(1);
            }
            ExitProcess(0);
#else
            #error Unsupported platform
#endif
        } else {
            shl_debug("Up to date: %s\n", out);
#if !defined(_WIN32) && !defined(_WIN64)
            free(out);
#endif
        }
    }

    // Helper: Extract source and output from command array
    // Assumes format: [compiler, flags..., source, "-o", output, ...]
    static const char* shl_cmd_get_source(SHL_Cmd* cmd) {
        if (!cmd || !cmd->data || cmd->len < 2) return NULL;
        
        // Find "-o" flag
        for (size_t i = 0; i < cmd->len - 1; i++) {
            if (cmd->data[i] && strcmp(cmd->data[i], "-o") == 0) {
                // Source should be before "-o"
                if (i > 0) {
                    // Skip compiler and flags, find first .c/.cpp file
                    for (size_t j = 1; j < i; j++) {
                        if (cmd->data[j] && strstr(cmd->data[j], ".c") != NULL) {
                            return cmd->data[j];
                        }
                    }
                    // Fallback: return item before "-o"
                    return cmd->data[i - 1];
                }
            }
        }
        
        // Fallback: find first .c file
        for (size_t i = 1; i < cmd->len; i++) {
            if (cmd->data[i] && strstr(cmd->data[i], ".c") != NULL) {
                return cmd->data[i];
            }
        }
        
        return NULL;
    }

    static const char* shl_cmd_get_output(SHL_Cmd* cmd) {
        if (!cmd || !cmd->data || cmd->len < 2) return NULL;
        
        // Find "-o" flag
        for (size_t i = 0; i < cmd->len - 1; i++) {
            if (cmd->data[i] && strcmp(cmd->data[i], "-o") == 0) {
                // Output is after "-o"
                return cmd->data[i + 1];
            }
        }
        
        return NULL;
    }

    // Execute command array using system() - builds command string
    static bool shl_cmd_execute(SHL_Cmd* cmd) {
        if (!cmd || !cmd->data || cmd->len == 0) {
            shl_log(SHL_LOG_ERROR, "Invalid command: empty or null\n");
            return false;
        }

        // Build command string
        char command[4096] = {0};
        size_t pos = 0;
        
        for (size_t i = 0; i < cmd->len; i++) {
            if (!cmd->data[i]) continue; // Skip NULL items
            if (pos > 0 && pos < sizeof(command) - 1) {
                command[pos++] = ' ';
            }
            const char *item = cmd->data[i];
            size_t item_len = strlen(item);
            
            // Check if item needs quoting (contains spaces)
            bool needs_quote = strchr(item, ' ') != NULL;
            
            if (needs_quote && pos < sizeof(command) - 2) {
                command[pos++] = '"';
            }
            
            if (pos + item_len < sizeof(command) - 1) {
                strncpy(command + pos, item, sizeof(command) - pos - 1);
                pos += item_len;
            }
            
            if (needs_quote && pos < sizeof(command) - 1) {
                command[pos++] = '"';
            }
        }
        
        command[pos] = '\0';
        
        shl_log(SHL_LOG_CMD, "%s\n", command);
        int result = system(command);
        
        if (result != 0) {
            shl_log(SHL_LOG_ERROR, "Command failed with exit code %d.\n", result);
            return false;
        }
        
        return true;
    }

    bool shl_run(SHL_Cmd* config) {
        if (!config || !config->data || config->len == 0) {
            shl_log(SHL_LOG_ERROR, "Invalid build configuration\n");
            if (config) shl_release(config);
            return false;
        }

        const char *source = shl_cmd_get_source(config);
        const char *output = shl_cmd_get_output(config);
        
        if (!source || !output) {
            shl_log(SHL_LOG_ERROR, "Could not extract source or output from command\n");
            shl_release(config);
            return false;
        }

        shl_ensure_dir_for_file(output);

        // Check if rebuild is needed
        if (!shl_is_path1_modified_after_path2(source, output)) {
            shl_log(SHL_LOG_DEBUG, "Up to date: %s\n", output);
            shl_release(config);
            return true; // Already up to date
        }

        bool res = shl_run_always(config);
        shl_release(config);
        return res;
    }

    bool shl_run_always(SHL_Cmd* config) {
        if (!config || !config->data || config->len == 0) {
            shl_log(SHL_LOG_ERROR, "Invalid build configuration\n");
            if (config) shl_release(config);
            return false;
        }

        bool res = shl_cmd_execute(config);
        shl_release(config);
        return res;
    }

    //////////////////////////////////////////////////
    /// FILE_OPS /////////////////////////////////////
    //////////////////////////////////////////////////

    bool shl_mkdir_if_not_exists(const char *path) {
        struct stat st;
#if defined(_WIN32) || defined(_WIN64)
        if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES) {
            return true;
        }
#else
        if (stat(path, &st) == 0) {
            return true;
        }
#endif
        return shl_mkdir(path);
    }

    bool shl_mkdir(const char *path) {
#ifdef _WIN32
        int result = _mkdir(path);
#else
        int result = mkdir(path, 0755);
#endif
        if (result != 0) {
            shl_log(SHL_LOG_ERROR, "Failed to create directory: %s\n", path);
            return false;
        }
        shl_log(SHL_LOG_DEBUG, "created directory `%s/`\n", path);
        return true;
    }

    bool shl_copy_file(const char *src_path, const char *dst_path) {
        if (!src_path || !dst_path) return false;

        FILE *src = fopen(src_path, "rb");
        if (!src) {
            shl_log(SHL_LOG_ERROR, "Failed to open source file: %s\n", src_path);
            return false;
        }

        FILE *dst = fopen(dst_path, "wb");
        if (!dst) {
            shl_log(SHL_LOG_ERROR, "Failed to open destination file: %s\n", dst_path);
            fclose(src);
            return false;
        }

        char buffer[4096];
        size_t bytes_read;
        while ((bytes_read = fread(buffer, 1, sizeof(buffer), src)) > 0) {
            if (fwrite(buffer, 1, bytes_read, dst) != bytes_read) {
                shl_log(SHL_LOG_ERROR, "Failed to write to destination file\n");
                fclose(src);
                fclose(dst);
                return false;
            }
        }

        fclose(src);
        fclose(dst);
        shl_log(SHL_LOG_DEBUG, "Copied %s to %s\n", src_path, dst_path);
        return true;
    }

    bool shl_copy_dir_rec(const char *src_path, const char *dst_path) {
        if (!src_path || !dst_path) return false;

#if (defined(__APPLE__) && defined(__MACH__)) || defined(__linux__)
        DIR *dir = opendir(src_path);
        if (!dir) {
            shl_log(SHL_LOG_ERROR, "Failed to open source directory: %s\n", src_path);
            return false;
        }

        if (!shl_mkdir_if_not_exists(dst_path)) {
            closedir(dir);
            return false;
        }

        struct dirent *entry;
        char src_file[1024];
        char dst_file[1024];

        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;

            snprintf(src_file, sizeof(src_file), "%s/%s", src_path, entry->d_name);
            snprintf(dst_file, sizeof(dst_file), "%s/%s", dst_path, entry->d_name);

            struct stat st;
            if (stat(src_file, &st) == 0) {
                if (S_ISDIR(st.st_mode)) {
                    if (!shl_copy_dir_rec(src_file, dst_file)) {
                        closedir(dir);
                        return false;
                    }
                } else if (S_ISREG(st.st_mode)) {
                    if (!shl_copy_file(src_file, dst_file)) {
                        closedir(dir);
                        return false;
                    }
                }
            }
        }

        closedir(dir);
        return true;
#elif defined(_WIN32) || defined(_WIN64)
        WIN32_FIND_DATA find_data;
        char search_path[1024];
        snprintf(search_path, sizeof(search_path), "%s\\*", src_path);

        HANDLE handle = FindFirstFile(search_path, &find_data);
        if (handle == INVALID_HANDLE_VALUE) {
            shl_log(SHL_LOG_ERROR, "Failed to open source directory: %s\n", src_path);
            return false;
        }

        if (!shl_mkdir_if_not_exists(dst_path)) {
            FindClose(handle);
            return false;
        }

        do {
            if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0)
                continue;

            char src_file[1024];
            char dst_file[1024];
            snprintf(src_file, sizeof(src_file), "%s\\%s", src_path, find_data.cFileName);
            snprintf(dst_file, sizeof(dst_file), "%s\\%s", dst_path, find_data.cFileName);

            if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                if (!shl_copy_dir_rec(src_file, dst_file)) {
                    FindClose(handle);
                    return false;
                }
            } else {
                if (!shl_copy_file(src_file, dst_file)) {
                    FindClose(handle);
                    return false;
                }
            }
        } while (FindNextFile(handle, &find_data));

        FindClose(handle);
        return true;
#else
        #error Unsupported platform
#endif
    }

    bool shl_read_file(const char *path, SHL_String *content) {
        if (!path || !content) return false;

        FILE *fp = fopen(path, "r");
        if (!fp) return false;

        char *line = NULL;
        size_t n = 0;

        while (getline(&line, &n, fp) != -1) {
            size_t len = strlen(line);

            if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';

            char *copy = strdup(line);
            if (!copy) {
                fclose(fp);
                free(line);
                return false;
            }

            shl_push(content, copy);
        }

        free(line);
        fclose(fp);
        return true;
    }

    bool shl_read_dir(const char *parent, const char *children) {
        if (!parent || !children) return false;
        SHL_UNUSED(children); // Reserved for future filtering

#if (defined(__APPLE__) && defined(__MACH__)) || defined(__linux__)
        DIR *dir = opendir(parent);
        if (!dir) {
            shl_log(SHL_LOG_ERROR, "Failed to open directory: %s\n", parent);
            return false;
        }

        struct dirent *entry;
        shl_log(SHL_LOG_INFO, "Contents of %s:\n", parent);
        while ((entry = readdir(dir)) != NULL) {
            struct stat st;
            char full_path[1024];
            snprintf(full_path, sizeof(full_path), "%s/%s", parent, entry->d_name);

            if (stat(full_path, &st) == 0) {
                if (S_ISDIR(st.st_mode)) {
                    shl_log(SHL_LOG_INFO, "  [DIR]  %s\n", entry->d_name);
                } else if (S_ISREG(st.st_mode)) {
                    shl_log(SHL_LOG_INFO, "  [FILE] %s (%zu bytes)\n", entry->d_name, (size_t)st.st_size);
                } else {
                    shl_log(SHL_LOG_INFO, "  [????] %s\n", entry->d_name);
                }
            }
        }

        closedir(dir);
        return true;
#elif defined(_WIN32) || defined(_WIN64)
        WIN32_FIND_DATA find_data;
        char search_path[1024];
        snprintf(search_path, sizeof(search_path), "%s\\*", parent);

        HANDLE handle = FindFirstFile(search_path, &find_data);
        if (handle == INVALID_HANDLE_VALUE) {
            shl_log(SHL_LOG_ERROR, "Failed to open directory: %s\n", parent);
            return false;
        }

        shl_log(SHL_LOG_INFO, "Contents of %s:\n", parent);
        do {
            if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                shl_log(SHL_LOG_INFO, "  [DIR]  %s\n", find_data.cFileName);
            } else {
                shl_log(SHL_LOG_INFO, "  [FILE] %s (%lu bytes)\n",
                        find_data.cFileName, find_data.nFileSizeLow);
            }
        } while (FindNextFile(handle, &find_data));

        FindClose(handle);
        return true;
#else
        #error Unsupported platform
#endif
    }

    bool shl_write_file(const char *path, const void *data, size_t size) {
        if (!path || !data) return false;

        FILE *fp = fopen(path, "wb");
        if (!fp) {
            shl_log(SHL_LOG_ERROR, "Failed to open file for writing: %s\n", path);
            return false;
        }

        size_t written = fwrite(data, 1, size, fp);
        fclose(fp);

        if (written != size) {
            shl_log(SHL_LOG_ERROR, "Failed to write all data to file: %s\n", path);
            return false;
        }

        shl_log(SHL_LOG_DEBUG, "Wrote %zu bytes to %s\n", written, path);
        return true;
    }

    const char *shl_get_file_type(const char *path) {
        if (!path) return "unknown";

        const char *dot = strrchr(path, '.');
        if (!dot || dot == path) return "no_ext";

        return dot + 1; // Returns extension without the dot
    }

    bool shl_delete_file(const char *path) {
        if (!path) return false;

#if (defined(__APPLE__) && defined(__MACH__)) || defined(__linux__)
        if (unlink(path) != 0) {
            shl_log(SHL_LOG_ERROR, "Failed to delete file: %s\n", path);
            return false;
        }

        shl_log(SHL_LOG_DEBUG, "Deleted file: %s\n", path);
        return true;
#elif defined(_WIN32) || defined(_WIN64)
        if (DeleteFile(path) == 0) {
            shl_log(SHL_LOG_ERROR, "Failed to delete file: %s\n", path);
            return false;
        }

        shl_log(SHL_LOG_DEBUG, "Deleted file: %s\n", path);
        return true;
#else
        #error Unsupported platform
#endif
    }

    void shl_release_string(SHL_String* content) {
        if (!content || !content->data) return;

        for (size_t i = 0; i < content->len; i++) {
            free(content->data[i]);
        }
        free(content->data);
        content->data = NULL;
        content->len = content->cap = 0;
    }

    //////////////////////////////////////////////////
    /// HASHMAP //////////////////////////////////////
    //////////////////////////////////////////////////

    static size_t shl_hm_hash(void* key, size_t key_size, size_t capacity) {
        size_t hash = 5381;
        const unsigned char *p = (const unsigned char *)key;
        for (size_t i = 0; i < key_size; i++) {
            hash = ((hash << 5) + hash) + p[i];
        }
        return hash % capacity;
    }

    static bool shl_hm_keys_equal(void* key1, void* key2) {
        size_t key_size = strlen(key1) + 1;
        return memcmp(key1, key2, key_size) == 0;
    }

    SHL_HashMap* shl_hm_create() {
        SHL_HashMap* hm = (SHL_HashMap*)calloc(1, sizeof(SHL_HashMap));
        if (!hm) return NULL;

        int initial_capacity = 4;
        hm->buckets = (SHL_HashMapEntry*)calloc(initial_capacity, sizeof(SHL_HashMapEntry));
        if (!hm->buckets) {
            free(hm);
            return NULL;
        }

        hm->capacity = initial_capacity;
        hm->size = 0;
        return hm;
    }

    static void shl_hm_resize(SHL_HashMap* hm) {
        size_t old_capacity = hm->capacity;
        SHL_HashMapEntry* old_buckets = hm->buckets;

        hm->capacity = hm->capacity * 2;
        hm->buckets = (SHL_HashMapEntry*)calloc(hm->capacity, sizeof(SHL_HashMapEntry));
        if (!hm->buckets) {
            hm->buckets = old_buckets;
            hm->capacity = old_capacity;
            shl_log(SHL_LOG_ERROR, "Failed to resize hashmap\n");
            return;
        }

        size_t new_size = 0;
        for (size_t i = 0; i < old_capacity; i++) {
            if (old_buckets[i].state == SHL_HM_USED) {
                size_t hash = shl_hm_hash(old_buckets[i].key, old_buckets[i].key_size, hm->capacity);
                size_t index = hash;

                // Linear probing to find empty slot
                while (hm->buckets[index].state == SHL_HM_USED) {
                    index = (index + 1) % hm->capacity;
                    if (index == hash) {
                        shl_log(SHL_LOG_ERROR, "Hashmap table is full during resize\n");
                        break;
                    }
                }

                if (hm->buckets[index].state != SHL_HM_USED) {
                    hm->buckets[index].key = old_buckets[i].key;
                    hm->buckets[index].value = old_buckets[i].value;
                    hm->buckets[index].key_size = old_buckets[i].key_size;
                    hm->buckets[index].value_size = old_buckets[i].value_size;
                    hm->buckets[index].state = SHL_HM_USED;
                    new_size++;
                }
            }
        }

        free(old_buckets);
        hm->size = new_size;
        shl_log(SHL_LOG_DEBUG, "Hashmap resized to %zu buckets\n", hm->capacity);
    }

    void shl_hm_put(SHL_HashMap* hm, void* key, void* value) {
        if (!hm || !key || !value) return;

        // Keys are always strings (null-terminated)
        size_t key_size = strlen(key) + 1;

        // NOTE: Values are stored as pointers only, not copied
        // This assumes caller manages value lifetime
        size_t value_size = sizeof(void*);

        // Resize if load factor > 0.75
        if (hm->size * 4 > hm->capacity * 3) {
            shl_hm_resize(hm);
        }

        size_t hash = shl_hm_hash(key, key_size, hm->capacity);
        size_t index = hash;

        // Linear probing
        while (hm->buckets[index].state != SHL_HM_EMPTY) {
            if (hm->buckets[index].state == SHL_HM_USED && shl_hm_keys_equal(hm->buckets[index].key, key)) {
                shl_log(SHL_LOG_DEBUG, "Updating entry for key: %s\n", (const char*)key);
                // Update existing value - just store the pointer
                free(hm->buckets[index].value);
                hm->buckets[index].value = malloc(value_size);
                if (hm->buckets[index].value) {
                    memcpy(hm->buckets[index].value, &value, value_size);
                    hm->buckets[index].value_size = value_size;
                }
                return;
            }
            index = (index + 1) % hm->capacity;
            if (index == hash) {
                shl_log(SHL_LOG_ERROR, "Hashmap table is full\n");
                return;
            }
        }

        // Found empty or deleted slot
        if (hm->buckets[index].state == SHL_HM_EMPTY || hm->buckets[index].state == SHL_HM_DELETED) {
            shl_log(SHL_LOG_DEBUG, "Inserting new entry for key: %s\n", (const char*)key);

            hm->buckets[index].key = malloc(key_size);
            hm->buckets[index].value = malloc(value_size);

            if (!hm->buckets[index].key || !hm->buckets[index].value) {
                if (hm->buckets[index].key) free(hm->buckets[index].key);
                if (hm->buckets[index].value) free(hm->buckets[index].value);
                shl_log(SHL_LOG_ERROR, "Failed to allocate memory for hashmap entry\n");
                return;
            }

            memcpy(hm->buckets[index].key, key, key_size);
            // Store the pointer itself, not the value it points to
            memcpy(hm->buckets[index].value, &value, value_size);
            hm->buckets[index].key_size = key_size;
            hm->buckets[index].value_size = value_size;
            hm->buckets[index].state = SHL_HM_USED;
            hm->size++;
        }
    }

    void* shl_hm_get(SHL_HashMap* hm, void* key) {
        if (!hm || !key) return NULL;

        size_t key_size = strlen(key) + 1;
        size_t hash = shl_hm_hash(key, key_size, hm->capacity);
        size_t index = hash;

        // Linear probing
        while (hm->buckets[index].state != SHL_HM_EMPTY) {
            if (hm->buckets[index].state == SHL_HM_USED && shl_hm_keys_equal(hm->buckets[index].key, key)) {
                // value is a pointer to the actual value pointer
                void** value_ptr = (void**)hm->buckets[index].value;
                return value_ptr ? *value_ptr : NULL;
            }
            index = (index + 1) % hm->capacity;
            if (index == hash) break; // Searched the entire table
        }

        return NULL;
    }

    bool shl_hm_contains(SHL_HashMap* hm, void* key) {
        return shl_hm_get(hm, key) != NULL ? true : false;
    }

    bool shl_hm_remove(SHL_HashMap* hm, void* key) {
        if (!hm || !key) return false;

        size_t key_size = strlen(key) + 1;
        size_t hash = shl_hm_hash(key, key_size, hm->capacity);
        size_t index = hash;

        // Linear probing
        while (hm->buckets[index].state != SHL_HM_EMPTY) {
            if (hm->buckets[index].state == SHL_HM_USED && shl_hm_keys_equal(hm->buckets[index].key, key)) {
                // Mark as deleted
                free(hm->buckets[index].key);
                free(hm->buckets[index].value);
                hm->buckets[index].key = NULL;
                hm->buckets[index].value = NULL;
                hm->buckets[index].state = SHL_HM_DELETED;
                hm->size--;
                return true;
            }
            index = (index + 1) % hm->capacity;
            if (index == hash) break;
        }

        return false;
    }

    void shl_hm_clear(SHL_HashMap* hm) {
        if (!hm) return;

        for (size_t i = 0; i < hm->capacity; i++) {
            if (hm->buckets[i].state == SHL_HM_USED) {
                free(hm->buckets[i].key);
                free(hm->buckets[i].value);
                hm->buckets[i].key = NULL;
                hm->buckets[i].value = NULL;
                hm->buckets[i].state = SHL_HM_EMPTY;
            }
        }
        hm->size = 0;
    }

    void shl_hm_release(SHL_HashMap* hm) {
        if (!hm) return;
        shl_hm_clear(hm);
        free(hm->buckets);
        free(hm);
    }

    size_t shl_hm_size(SHL_HashMap* hm) {
        return hm ? hm->size : 0;
    }

    bool shl_hm_empty(SHL_HashMap* hm) {
        return !hm || hm->size == 0;
    }

    //////////////////////////////////////////////////
    /// UNITTEST /////////////////////////////////////
    //////////////////////////////////////////////////

    typedef struct {
        shl_test_t tests[1024];
        size_t count;
        size_t passed;
        size_t failed;
    } shl_test_suite_t;

    static shl_test_suite_t shl_test_suite = {0};
    char shl_test_failure_msg[256] = {0};

    void shl_test_register(const char *name, const char *file, int line, void (*test_func)(void)) {
        if (shl_test_suite.count >= SHL_ARRAY_LEN(shl_test_suite.tests)) {
            fprintf(stderr, "Too many tests registered!\n");
            return;
        }
        shl_test_t *test = &shl_test_suite.tests[shl_test_suite.count++];
        test->name = name;
        test->file = file;
        test->line = line;
        test->func = test_func;
    }

    static bool shl_test_current_failed = false;

    void shl_test_fail(void) {
        shl_test_current_failed = true;
    }

    int shl_test_run_all(void) {
        size_t test_count = shl_test_suite.count;
        shl_test_suite.passed = 0;
        shl_test_suite.failed = 0;

        // Find the longest test name for alignment
        size_t max_name_len = 0;
        for (size_t i = 0; i < test_count; i++) {
            size_t len = strlen(shl_test_suite.tests[i].name);
            if (len > max_name_len) max_name_len = len;
        }

        const size_t target_width = 60;
        const size_t prefix_len = 7; // "[TEST] " prefix

        for (size_t i = 0; i < test_count; i++) {
            shl_test_t *test = &shl_test_suite.tests[i];
            shl_test_current_failed = false;
            shl_test_failure_msg[0] = '\0'; // Reset failure message

            // Calculate dots needed to reach alignment point
            size_t name_len = strlen(test->name);
            size_t total_prefix = prefix_len + name_len;
            size_t space_needed = target_width - total_prefix;
            // size_t dots_needed = total_prefix < target_width ? target_width - total_prefix : 10;
            size_t dots_needed = space_needed;

            printf("[TEST] %s ", test->name);

            // Print dots for alignment
            for (size_t j = 0; j < dots_needed; j++) {
                printf(".");
            }

            // Run the test
            test->func();

            // Print result on same line with colors
            if (shl_test_current_failed) {
                printf("\033[31m [FAILED]\033[0m\n"); // Red
                if (shl_test_failure_msg[0] != '\0') {
                    printf("  %s\n", shl_test_failure_msg);
                }
                shl_test_suite.failed++;
            } else {
                printf("\033[32m [OK]\033[0m\n"); // Green
                shl_test_suite.passed++;
            }
        }

        printf("Total: %zu, Passed: %zu, Failed: %zu\n", shl_test_suite.count, shl_test_suite.passed, shl_test_suite.failed);

        return shl_test_suite.failed > 0 ? 1 : 0;
    }

    //////////////////////////////////////////////////
    /// TIMER ////////////////////////////////////////
    //////////////////////////////////////////////////

    void shl_timer_start(SHL_Timer *timer) {
        if (!timer) return;

#if defined(_WIN32) || defined(_WIN64)
        QueryPerformanceFrequency(&timer->frequency);
        QueryPerformanceCounter(&timer->start);
#else
        clock_gettime(CLOCK_MONOTONIC, &timer->start);
#endif
    }

    double shl_timer_elapsed(SHL_Timer *timer) {
        if (!timer) return 0.0;

#if defined(_WIN32) || defined(_WIN64)
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        return (double)(now.QuadPart - timer->start.QuadPart) / (double)timer->frequency.QuadPart;
#else
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        double elapsed = (double)(now.tv_sec - timer->start.tv_sec) +
                         (double)(now.tv_nsec - timer->start.tv_nsec) / 1e9;
        return elapsed;
#endif
    }

    double shl_timer_elapsed_ms(SHL_Timer *timer) {
        return shl_timer_elapsed(timer) * 1000.0;
    }

    double shl_timer_elapsed_us(SHL_Timer *timer) {
        return shl_timer_elapsed(timer) * 1000000.0;
    }

    uint64_t shl_timer_elapsed_ns(SHL_Timer *timer) {
        if (!timer) return 0;

#if defined(_WIN32) || defined(_WIN64)
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        uint64_t elapsed_ticks = (uint64_t)(now.QuadPart - timer->start.QuadPart);
        // Convert to nanoseconds: (ticks / frequency) * 1e9
        return (uint64_t)((double)elapsed_ticks * 1e9 / (double)timer->frequency.QuadPart);
#else
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        uint64_t elapsed_sec = (uint64_t)(now.tv_sec - timer->start.tv_sec);
        int64_t elapsed_nsec = (int64_t)(now.tv_nsec - timer->start.tv_nsec);
        return elapsed_sec * 1000000000ULL + (uint64_t)elapsed_nsec;
#endif
    }

    void shl_timer_reset(SHL_Timer *timer) {
        if (!timer) return;
        shl_timer_start(timer);
    }

#endif // SHL_IMPLEMENTATION

//////////////////////////////////////////////////
/// SHL_STRIP_PREFIX /////////////////////////////
//////////////////////////////////////////////////

#ifdef SHL_STRIP_PREFIX

    // HELPER
    #define ASSERT                  SHL_ASSERT
    #define UNUSED                  SHL_UNUSED
    #define TODO                    SHL_TODO
    #define UNREACHABLE             SHL_UNREACHABLE
    #define ARRAY_LEN               SHL_ARRAY_LEN
    #define ARRAY_GET               SHL_ARRAY_GET

    // LOGGER
    #define init_logger             shl_init_logger
    #define debug                   shl_debug
    #define info                    shl_info
    #define cmd                     shl_cmd
    #define hint                    shl_hint
    #define warn                    shl_warn
    #define error                   shl_error
    #define critical                shl_critical
    #define LOG_NONE                SHL_LOG_NONE
    #define LOG_DEBUG               SHL_LOG_DEBUG
    #define LOG_INFO                SHL_LOG_INFO
    #define LOG_CMD                 SHL_LOG_CMD
    #define LOG_HINT                SHL_LOG_HINT
    #define LOG_WARN                SHL_LOG_WARN
    #define LOG_ERROR               SHL_LOG_ERROR
    #define LOG_CRITICAL            SHL_LOG_CRITICAL

    // CLI_PARSER
    #define init_argparser          shl_init_argparser
    #define add_argument            shl_add_argument
    #define get_argument            shl_get_argument
    #define shift                   shl_shift
    #define arg_t                   shl_arg_t

    // NO_BUILD
    #define CmdTask                 SHL_CmdTask
    #define auto_rebuild            shl_auto_rebuild
    #define auto_rebuild_plus       shl_auto_rebuild_plus
    #define get_filename_no_ext     shl_get_filename_no_ext
    #define default_compiler_flags  shl_default_compiler_flags
    #define default_c_build         shl_default_c_build
    #define run                     shl_run
    #define run_always              shl_run_always 
    #define Cmd                     SHL_Cmd

    // DYN_ARRAY
    #define grow                    shl_grow
    #define shrink                  shl_shrink
    #define push                    shl_push
    #define drop                    shl_drop
    #define dropn                   shl_dropn
    #define resize                  shl_resize
    #define release                 shl_release
    #define back                    shl_back
    #define swap                    shl_swap
    #define list                    shl_list

    // HELPER
    #define ASSERT                  SHL_ASSERT
    #define UNUSED                  SHL_UNUSED
    #define TODO                    SHL_TODO
    #define UNREACHABLE             SHL_UNREACHABLE
    #define ARRAY_LEN               SHL_ARRAY_LEN
    #define ARRAY_GET               SHL_ARRAY_GET

    // FILE_OPS
    #define String                  SHL_String
    #define mkdir                   shl_mkdir
    #define mkdir_if_not_exists     shl_mkdir_if_not_exists
    #define copy_file               shl_copy_file
    #define copy_dir_rec            shl_copy_dir_rec
    #define read_dir                shl_read_dir
    #define read_file               shl_read_file
    #define write_file              shl_write_file
    #define get_file_type           shl_get_file_type
    #define delete_file             shl_delete_file
    #define release_string          shl_release_string

    // HASHMAP
    #define HashMap                 SHL_HashMap
    #define HashMapEntry            SHL_HashMapEntry
    #define hm_create               shl_hm_create
    #define hm_put                  shl_hm_put
    #define hm_get                  shl_hm_get
    #define hm_contains             shl_hm_contains
    #define hm_remove               shl_hm_remove
    #define hm_clear                shl_hm_clear
    #define hm_release              shl_hm_release
    #define hm_size                 shl_hm_size
    #define hm_empty                shl_hm_empty

    // UNITTEST
    #define Test                    shl_test_t
    #define test_register           shl_test_register
    #define test_run_all            shl_test_run_all
    #define test_print_summary      shl_test_print_summary
    #define TEST_ASSERT             SHL_TEST_ASSERT
    #define TEST_EQ                 SHL_TEST_EQ
    #define TEST_NEQ                SHL_TEST_NEQ
    #define TEST_STREQ              SHL_TEST_STREQ
    #define TEST_STRNEQ             SHL_TEST_STRNEQ
    #define TEST_TRUTHY             SHL_TEST_TRUTHY
    #define TEST_FALSY              SHL_TEST_FALSY
    #define TEST                    SHL_TEST

    // TIMER
    #define Timer                   SHL_Timer
    #define timer_start             shl_timer_start
    #define timer_elapsed           shl_timer_elapsed
    #define timer_elapsed_ms        shl_timer_elapsed_ms
    #define timer_elapsed_us        shl_timer_elapsed_us
    #define timer_elapsed_ns        shl_timer_elapsed_ns
    #define timer_reset             shl_timer_reset

#endif // SHL_STRIP_PREFIX

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // SHL_BUILD_H
