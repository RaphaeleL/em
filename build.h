/* build.h - v0.0.6 - https://github.com/RaphaeleL/build.h
   ============================================================================
    File: build.h
    Description: Quality-of-life utilities and abstractions for C development.

    This header provides a collection of macros, inline utilities, build-time
    helpers and more, intended to simplify common C programming patterns,
    improve code clarity, and enhance portability across compilers and
    platforms.

    The Idea could be considered as a mix between nothings/stb and tsoding/nob.h

    ----------------------------------------------------------------------------
    Created : 02 Oct 2025
    Changed : 14 Jul 2026
    Author  : Raphaele Salvatore Licciardo, M.Sc.
    License : MIT
    Version : 0.0.6 WIP
    ----------------------------------------------------------------------------

    Quick Example: Auto Rebuild the Build System

      // build.c
      #define QOL_IMPLEMENTATION
      #define QOL_STRIP_PREFIX
      #include "./build.h"

      int main() {
          auto_rebuild(__FILE__);

          Cmd build = default_c_build("demo.c", "demo");
          if (!run(&build)) {  // auto-releases on success or failure
              return EXIT_FAILURE;
          }
          // -> run `cc -o demo demo.c` on change of source file

          Cmd calc = default_c_build("calc.c", NULL);
          push(&calc, "-Wall", "-Wextra");
          if (!qol_run_always(&calc)) {  // auto-releases on success or failure
              return EXIT_FAILURE;
          }
          // -> run `cc -Wall -Wextra calc.c -o calc` always

          return EXIT_SUCCESS;
      }

    Further Example: Build System, Arg Parser, Helpers, Hashmap, Logger

      // demo.c
      #define QOL_IMPLEMENTATION
      #define QOL_STRIP_PREFIX
      #include "./build.h"

      int main(int argc, char** argv) {
          auto_rebuild_plus("demo.c", "build.h");

          const char* progr = shift(argc, argv);
          const char* param = shift(argc, argv);

          UNUSED(progr);

          if (strcmp(param, "hashmap") == 0) {
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
          return EXIT_SUCCESS;
      }

    ----------------------------------------------------------------------------

    Changelog:

      0.0.1 - 08.12.2025
        - Initial release

      0.0.2 - 21.12.2025
        - prevent buffer overflow
        - prevent memory leak
        - prevent inconsistent state after several failures
        - improve the error handling
        - readability in logfile
        - add string utilities, date and datetime
        - rename the log levels and redefine their behavior

      0.0.3 - 03.01.2026
        - overall thread safety improvements
        - make qol_read_dir() saving full paths
        - add qol_read_dir_recursive() to read dirs recursively
        - fix mutex forward declaration location

      0.0.4 - 22.04.2026
        - automatic memory release for gcc and clang
        - redesign the logger api
        - prevent qol_run() from detecting c flags, always
        - workaround for the unittest alignment issue

      0.0.5 - 14.07.2026 
        - implement a default c extend build
        - fix platform detection reporting is_windows=true on macOS/Linux
        - fix use-after-free of auto-generated output names in default builds
        - re-enable ANSI support on Windows (stdout and stderr)
        - hashmap: reuse deleted slots on insert, safe key comparison
        - unittest: fix runaway dot padding for long test names
        - unittest: add qol_test_print_summary implementation
        - temp allocator: 16-byte alignment and overflow-safe capacity check
        - logger: no escape codes when colors are disabled
        - define _POSIX_C_SOURCE before system includes
        - use time_t for rebuild timestamp comparisons

      0.0.6 - wip 
        - todo

    ----------------------------------------------------------------------------
    Copyright (c) 2026 Raphaele Salvatore Licciardo

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

#ifndef QOL_BUILD_H  // include guard
#define QOL_BUILD_H

// Ensure POSIX.1-2008 features are available (clock_gettime, getline, strdup).
// This must be defined before ANY system header is included, otherwise the
// feature test macro has no effect on already-included headers.
#if !defined(_WIN32) && !defined(_WIN64)
    #ifndef _POSIX_C_SOURCE
        #define _POSIX_C_SOURCE 200809L
    #endif
#endif

// Check for C++ compilation and use C linkage for compatibility
#ifdef __cplusplus
    extern "C" {
#endif // __cplusplus

#include <stdio.h>      // Standard input/output functions
#include <stdlib.h>     // Memory and process utilities
#include <string.h>     // String manipulation functions
#include <stdarg.h>     // Variable argument handling
#include <stdbool.h>    // Boolean type support
#include <stdint.h>     // Fixed-width integer types
#include <time.h>       // Time and date utilities
#include <ctype.h>      // Character classification helpers
#include <sys/stat.h>   // File status/statistics functions
#include <sys/types.h>  // System data type definitions
#include <errno.h>      // Error codes and messages
#include <limits.h>     // System-specific limits

#ifndef QOLDEF // Goes before declarations & definitions in case of `static inline`
#define QOLDEF
#endif /* QOLDEF */

// Custom assertion macro - can be overridden by defining QOL_ASSERT before including this header
// Defaults to standard assert() from assert.h. Useful for custom assertion handling in tests.
#ifndef QOL_ASSERT
    #include <assert.h>
    #define QOL_ASSERT assert
#endif /* QOL_ASSERT */

// Platform detection: Normalize compiler-defined macros into consistent platform identifiers
// This allows the rest of the code to use WINDOWS, MACOS, LINUX instead of compiler-specific macros
// WINDOWS: Defined for both 32-bit and 64-bit Windows (_WIN32 covers both)
// MACOS: Defined for macOS/Darwin systems (requires both __APPLE__ and __MACH__)
// LINUX: Defined for Linux systems
// UNKNOWN: Fallback for unrecognized platforms (will cause compile error later)
#if defined(_WIN32) || defined(_WIN64)
    static bool qol_is_windows = true;
    static bool qol_is_linux = false;
    static bool qol_is_macos = false;
    static const char *qol_os_name = "Windows";
    #define WINDOWS 1
#elif defined(__APPLE__) && defined(__MACH__)
    static bool qol_is_windows = false;
    static bool qol_is_linux = false;
    static bool qol_is_macos = true;
    static const char *qol_os_name = "macOS";
    #define MACOS 1
#elif defined(__linux__)
    static bool qol_is_windows = false;
    static bool qol_is_linux = true;
    static bool qol_is_macos = false;
    static const char *qol_os_name = "Linux";
    #define LINUX 1
#else
    #define UNKNOWN 1
#endif

// Platform-specific includes: Include headers needed for each platform's functionality
// Unix-like systems (macOS and Linux) share similar APIs, so they're grouped together
#if defined(MACOS) || defined(LINUX)
    #include <pthread.h>      // Threading support (for future async features)
    #include <unistd.h>       // POSIX API: fork, exec, getcwd, etc.
    #include <dirent.h>       // Directory reading (opendir, readdir, etc.)
    #include <sys/wait.h>     // Process waiting (waitpid, WEXITSTATUS, etc.)
    #include <fcntl.h>        // File control operations
#elif defined(WINDOWS)
    // Exclude rarely-used Windows APIs to reduce compilation time and header bloat
    #define WIN32_LEAN_AND_MEAN  // Exclude rarely used APIs
    #define _WINUSER_            // Skip user interface header
    #define _WINGDI_             // Skip GDI graphics header
    #define _IMM_                // Skip input method header
    #define _WINCON_             // Skip console API header
    #include <windows.h>   // Core Windows API (processes, files, etc.)
    #include <io.h>        // File I/O (_mkdir, etc.)
    #include <direct.h>    // Directory operations (_mkdir, _chdir)
    #include <shellapi.h>  // Shell operations (for future features)
#else
    // Unsupported platform - fail compilation with clear error message
    #error Unsupported platform
#endif

//////////////////////////////////////////////////
/// ANSI COLORS //////////////////////////////////
//////////////////////////////////////////////////

#define QOL_RESET           "\x1b[0m"  // Reset
#define QOL_RESET_FG        "\x1b[39m"
#define QOL_RESET_BG        "\x1b[49m"

#define QOL_BOLD            "\x1b[1m"  // Text Attributes
#define QOL_DIM             "\x1b[2m"
#define QOL_ITALIC          "\x1b[3m"
#define QOL_UNDERLINE       "\x1b[4m"
#define QOL_INVERT          "\x1b[7m"
#define QOL_HIDE            "\x1b[8m"
#define QOL_STRIKE          "\x1b[9m"

#define QOL_FG_BLACK        "\x1b[30m"  // Foreground
#define QOL_FG_RED          "\x1b[31m"
#define QOL_FG_GREEN        "\x1b[32m"
#define QOL_FG_YELLOW       "\x1b[33m"
#define QOL_FG_BLUE         "\x1b[34m"
#define QOL_FG_MAGENTA      "\x1b[35m"
#define QOL_FG_CYAN         "\x1b[36m"
#define QOL_FG_WHITE        "\x1b[37m"

#define QOL_FG_BBLACK       "\x1b[90m"  // Bright Foreground
#define QOL_FG_BRED         "\x1b[91m"
#define QOL_FG_BGREEN       "\x1b[92m"
#define QOL_FG_BYELLOW      "\x1b[93m"
#define QOL_FG_BBLUE        "\x1b[94m"
#define QOL_FG_BMAGENTA     "\x1b[95m"
#define QOL_FG_BCYAN        "\x1b[96m"
#define QOL_FG_BWHITE       "\x1b[97m"

#define QOL_BG_BLACK        "\x1b[40m"  // Background
#define QOL_BG_RED          "\x1b[41m"
#define QOL_BG_GREEN        "\x1b[42m"
#define QOL_BG_YELLOW       "\x1b[43m"
#define QOL_BG_BLUE         "\x1b[44m"
#define QOL_BG_MAGENTA      "\x1b[45m"
#define QOL_BG_CYAN         "\x1b[46m"
#define QOL_BG_WHITE        "\x1b[47m"

#define QOL_BG_BBLACK       "\x1b[100m"  // Bright Background
#define QOL_BG_BRED         "\x1b[101m"
#define QOL_BG_BGREEN       "\x1b[102m"
#define QOL_BG_BYELLOW      "\x1b[103m"
#define QOL_BG_BBLUE        "\x1b[104m"
#define QOL_BG_BMAGENTA     "\x1b[105m"
#define QOL_BG_BCYAN        "\x1b[106m"
#define QOL_BG_BWHITE       "\x1b[107m"

#define QOL_FG256(n)        "\x1b[38;5;" #n "m" // 256-Color Support
#define QOL_BG256(n)        "\x1b[48;5;" #n "m"

#define _QOL_STR_HELPER(x) #x  // Truecolor (RGB Support)
#define _QOL_STR(x) _QOL_STR_HELPER(x)
#define QOL_FG_RGB(r,g,b)   "\x1b[38;2;" _QOL_STR(r) ";" _QOL_STR(g) ";" _QOL_STR(b) "m"
#define QOL_BG_RGB(r,g,b)   "\x1b[48;2;" _QOL_STR(r) ";" _QOL_STR(g) ";" _QOL_STR(b) "m"

// Enable ANSI color codes in Windows console. On Unix-like systems, this is a no-op.
// This function must be called on Windows before using ANSI color codes for proper display.
// It enables virtual terminal processing which allows ANSI escape sequences to work.
QOLDEF void QOL_enable_ansi(void);

/* Use: QOL_FG256(196) for bright red, QOL_BG256(21) for deep blue */
/* Use: QOL_FG_RGB(255, 128, 0) */

//////////////////////////////////////////////////
/// LOGGER ///////////////////////////////////////
//////////////////////////////////////////////////

// Log level enumeration: Defines severity levels for log messages
// Lower numbers = more verbose, higher numbers = more critical
// Messages below the minimum level set by qol_init_logger() are filtered out
typedef enum {
    QOL_LOG_DIAG = 0,  // Debug messages: Detailed information for debugging (most verbose)
    QOL_LOG_INFO,      // Info messages: General informational messages about program flow
    QOL_LOG_EXEC,      // Command messages: Logs executed shell commands (useful for build systems)
    QOL_LOG_HINT,      // Hint messages: Helpful suggestions or tips (not errors or warnings)
    QOL_LOG_WARN,      // Warning messages: Something unusual happened but execution can continue
    QOL_LOG_ERRO,      // Error messages: Something went wrong, execution continues (caller decides how to handle)
    QOL_LOG_DEAD,      // Critical messages: Severe error, program will exit(EXIT_FAILURE) after logging
    QOL_LOG_NONE       // No logging: Disables all logging (useful for release builds)
} qol_log_level_t;

// Argument bundle for logger initialization.
// Used via designated initializers through the qol_init_logger(...) macro.
typedef struct {
    qol_log_level_t level;      // Minimum log level to emit (ignored if only_set is true)
    qol_log_level_t only;       // Only log messages at this exact level
    bool only_set;              // Enable only mode - only messages at .only level will be logged
    bool color;                 // Enable ANSI color output
    bool time;                  // Prefix log messages with timestamps
    bool time_color;            // Enable ANSI color output for the timestamp
} qol_init_logger_arguments;

// Macro wrapper enabling named arguments via designated initializers.
// Expands to a call to qol_init_logger_impl() with a compound literal.
#define qol_init_logger(...) qol_init_logger_impl((qol_init_logger_arguments){ __VA_ARGS__ })

// Initialize the logging subsystem.
// This function is intended to be called through the qol_init_logger(...) macro,
// which allows named arguments using designated initializers.
// Must be called before using any logging functions. Defaults to INFO level if not initialized.
//
// Examples:
//   init_logger(.level=LOG_INFO);                         // Minimum level mode (INFO and above)
//   init_logger(.level=LOG_DIAG, .color=true);            // Show all messages with colors
//   init_logger(.only=LOG_WARN, .only_set=true);          // Only WARN messages
//   init_logger(.only=LOG_HINT, .only_set=true);          // Only HINT messages
//
QOLDEF void qol_init_logger_impl(qol_init_logger_arguments args);

// Configure logger to also write messages to a file. The file path format string uses printf-style formatting.
// format: printf-style format string for the log file path (e.g., "logs/app_%s.log", qol_get_time()).
// Supports variadic arguments for dynamic file naming. File is opened in append mode.
// If format is NULL, file logging is disabled. Path supports ~ expansion for home directory.
// Log file contains plain text without ANSI color codes, even if color is enabled for console.
QOLDEF void qol_init_logger_logfile(const char *format, ...);

// Get current time as a formatted string in format "HH-MM-SS".
// Returns pointer to a static buffer containing the formatted time string.
// Useful for generating timestamped filenames or log entries. Thread-safe for read operations.
QOLDEF const char *qol_get_time(void);

// Get current date as a formatted string in format "YYYY-MM-DD".
// Returns pointer to a static buffer containing the formatted date string.
// Useful for generating timestamped filenames or log entries. Thread-safe for read operations.
QOLDEF const char *qol_get_date(void);

// Get current date and time as a formatted string in format "YYYY-MM-DD_HH-MM-SS".
// Returns pointer to a static buffer containing the formatted datetime string.
// Useful for generating timestamped filenames or log entries. Thread-safe for read operations.
QOLDEF const char *qol_get_datetime(void);

// Log a message at the specified log level using printf-style formatting.
// level: Log level (DIAG, INFO, EXEC, HINT, WARN, ERRO, DEAD).
// fmt: printf-style format string with optional variadic arguments.
// Messages are filtered based on init_logger() settings:
//   - Minimum level mode: Messages at or above .level are logged
//   - Only mode (.only_set=true): Only messages at exactly .only level are logged
// DEAD level calls exit(EXIT_FAILURE) after logging; all other levels return normally.
// Logs to stderr by default, and to file if qol_init_logger_logfile() was configured.
QOLDEF void qol_log(qol_log_level_t level, const char *fmt, ...);

// Expand path: Replace ~ with home directory path (Unix shell-style path expansion)
// Supports: "~" -> home directory, "~/path" -> home/path
// Returns newly allocated string that caller must free, or NULL on error
// If ~ expansion fails (no home directory), returns original path as-is
QOLDEF char *qol_expand_path(const char *path);

// Macros to easify the usage of log, instead of log(level, fmt) we are offering
// are more intuitive way of logging level(fmt)
#define qol_diag(fmt, ...) qol_log(QOL_LOG_DIAG, fmt, ##__VA_ARGS__)
#define qol_info(fmt, ...) qol_log(QOL_LOG_INFO, fmt, ##__VA_ARGS__)
#define qol_exec(fmt, ...) qol_log(QOL_LOG_EXEC, fmt, ##__VA_ARGS__)
#define qol_hint(fmt, ...) qol_log(QOL_LOG_HINT, fmt, ##__VA_ARGS__)
#define qol_warn(fmt, ...) qol_log(QOL_LOG_WARN, fmt, ##__VA_ARGS__)
#define qol_erro(fmt, ...) qol_log(QOL_LOG_ERRO, fmt, ##__VA_ARGS__)
#define qol_dead(fmt, ...) qol_log(QOL_LOG_DEAD, fmt, ##__VA_ARGS__)

// TIME, DATE, DATETIME macro - returns current time as formatted string
#define QOL_TIME qol_get_time()
#define QOL_DATE qol_get_date()
#define QOL_DATETIME qol_get_datetime()

//////////////////////////////////////////////////
/// CLI_PARSER ///////////////////////////////////
//////////////////////////////////////////////////

// Maximum number of command-line arguments that can be registered
#define QOL_ARG_MAX 128

// Argument structure: Represents a single command-line argument definition
// This structure defines what arguments the program accepts and stores parsed values
typedef struct {
    const char *long_name;   // Long option name (e.g., "--output" or "--help")
    char short_name;         // Short option name (e.g., 'o' for "-o"), auto-derived from long_name[2]
    const char *default_val; // Default value as string if argument not provided, NULL for flags/optional args
    const char *help_msg;    // Help text displayed when --help is used, can be NULL
    const char *value;       // Parsed value from command line (or default_val), NULL if not set
} qol_arg_t;

// Argument parser structure: Container for all registered command-line arguments
// Stores up to QOL_ARG_MAX arguments. The count field tracks how many are registered.
// This is a global structure (qol_parser) that persists throughout program execution
typedef struct {
    qol_arg_t args[QOL_ARG_MAX];  // Array of argument definitions
    int count;                      // Number of arguments currently registered
} qol_argparser_t;

extern qol_argparser_t qol_parser;

// Initialize the CLI argument parser and parse command-line arguments from argc/argv.
// Parses all arguments and matches them against registered arguments (via qol_add_argument).
// Automatically adds --help argument. If --help is found, prints usage and exits.
// Long options (--option) and short options (-o) are supported. Values follow options.
// Should be called after registering all arguments with qol_add_argument().
QOLDEF void qol_init_argparser(int argc, char *argv[]);

// Register a command-line argument with the parser. Must be called before qol_init_argparser().
// long_name: Long option name (e.g., "--output"). Short name is auto-derived from 3rd character.
// default_val: Default value as string if argument is not provided, or NULL for flags.
// help_msg: Help text displayed when --help is used. Can be NULL.
// Arguments are stored in the global qol_parser structure. Maximum QOL_ARG_MAX arguments.
QOLDEF void qol_add_argument(const char *long_name, const char *default_val, const char *help_msg);

// Get a parsed argument by its long name. Returns pointer to qol_arg_t structure or NULL if not found.
// The returned structure contains the parsed value (or default) and other argument metadata.
// Use this after qol_init_argparser() to retrieve argument values. Check value field for result.
QOLDEF qol_arg_t *qol_get_argument(const char *long_name);

// Convert an argument's value to an integer. Returns the integer value, or EXIT_SUCCESS (0) if argument is NULL or has no value.
// Uses atoi() internally to parse the string value. Safe to call on NULL arguments.
QOLDEF int qol_arg_as_int(qol_arg_t *arg);

// Get an argument's value as a string. Returns the value string, or empty string "" if argument is NULL or has no value.
// Returns a pointer to the value field in the qol_arg_t structure. The string is valid as long as the argument structure exists.
QOLDEF const char *qol_arg_as_string(qol_arg_t *arg);

// Shift macro: Remove and return the first element from an array, decrementing the size
// This is a common pattern for processing command-line arguments or array elements sequentially
// Inspired by tsoding/nob.h - a useful utility for array processing
// Usage: const char *arg = qol_shift(argc, argv); // Removes first element, returns it, decrements argc
// Note: This macro modifies both size and elements, so use with caution
// The QOL_ASSERT ensures we don't shift from an empty array (would cause undefined behavior)
#define qol_shift(size, elements) (QOL_ASSERT((size) > 0), (size)--, *(elements)++)

//////////////////////////////////////////////////
/// NO_BUILD /////////////////////////////////////
//////////////////////////////////////////////////

// Maximum number of parallel tasks that can be tracked (legacy constant, not strictly enforced)
#define MAX_TASKS 32

// Process handle type: Platform-specific type for representing a running process
// On Windows: HANDLE (from Windows API) - opaque handle to a process object
// On Unix: int (process ID / PID) - integer process identifier from fork()
// This abstraction allows the same code to work on both platforms
#ifdef WINDOWS
    typedef HANDLE QOL_Proc;
    #define QOL_INVALID_PROC INVALID_HANDLE_VALUE  // Windows invalid handle constant
#else
    typedef int QOL_Proc;                          // Unix process ID (PID)
    #define QOL_INVALID_PROC (-1)                  // Invalid PID (negative values are invalid)
#endif

// Helper macro to check if a process handle is valid
// Returns true if process handle is valid, false if invalid
// Useful for checking return values from async command execution
#define QOL_PROC_IS_VALID(proc) ((proc) != QOL_INVALID_PROC)

// Dynamic array of process handles: Used to track multiple parallel processes
// When commands are executed asynchronously (async=true), their process handles are stored here
// Allows waiting on all processes together with qol_procs_wait()
// Uses dynamic array pattern: grows as needed, must be initialized to zero or use qol_grow()
typedef struct {
    QOL_Proc *data;  // Array of process handles (allocated dynamically)
    size_t len;       // Number of processes currently tracked
    size_t cap;       // Capacity of the data array (for dynamic growth)
} QOL_Procs;

// Command structure: Represents a shell command as an array of arguments
// This is the core data structure for the build system - commands are built up and then executed
// The data array contains command and arguments: ["cc", "-Wall", "main.c", "-o", "main"]
// Uses dynamic array pattern: grows as arguments are added with qol_push()
typedef struct {
    const char **data;  // Array of command arguments (argv-style, NULL-terminated when executed)
    size_t len;         // Number of arguments currently in the command
    size_t cap;         // Capacity of the data array (for dynamic growth)
    bool async;         // If true, command runs asynchronously (returns immediately, handle in procs)
                        // If false, command runs synchronously (waits for completion before returning)
} QOL_Cmd;

// Run options structure: Configuration for how commands should be executed
// Currently only supports process tracking, but designed for future extensibility
typedef struct {
    QOL_Procs *procs;  // If provided and config->async=true, process handle is added to this array
                       // Allows tracking multiple parallel processes for later waiting
                       // Can be NULL if async process tracking is not needed
} QOL_RunOptions;

// Command task structure: Wrapper combining a command with its execution result
// Used internally for tracking command execution status (legacy structure, may be deprecated)
typedef struct {
    QOL_Cmd config;   // The command configuration to execute
    bool success;     // Whether the command execution succeeded (true) or failed (false)
} QOL_CmdTask;

// Get default compiler flags for the current platform as a string.
// Returns platform-specific flags: "-Wall -Wextra" on macOS/Linux, empty string on Windows.
// This is a static inline function for compile-time optimization.
QOLDEF char *qol_default_compiler_flags(void);

// Build a default C compilation command structure. Creates a QOL_Cmd with compiler, flags, source, and output.
// source: Path to source file (e.g., "main.c"). Required. Must be a trusted path - no validation is performed.
// output: Path to output executable, or NULL to auto-generate from source filename (without extension).
// Returns a QOL_Cmd structure ready to use with qol_run() or qol_run_always().
// On Windows uses "gcc", on Unix uses "cc". Adds -Wall -Wextra flags on Unix platforms.
// SECURITY NOTE: Paths are used directly in command execution without sanitization. Only use trusted paths
// from your application, not user input. Paths containing shell metacharacters could cause command injection.
QOLDEF QOL_Cmd qol_default_c_build(const char *source, const char *output);

// Build a default C compilation command structure, like qol_default_c_build, but with a custom compilter and
// custom flags.
// Returns a QOL_Cmd structure ready to use with qol_run() or qol_run_always().
QOLDEF QOL_Cmd qol_default_c_build_extended(const char *source, const char *output, const char *flags[], size_t flags_count, const char *compiler);

// Run a build command only if source files are newer than the output (incremental build).
// Checks modification times: if any source is newer than output, runs the command; otherwise skips.
// Usage: qol_run(&cmd) or qol_run(&cmd, (QOL_RunOptions){ .procs = &procs }).
// If config->async is true and opts.procs is provided, process handle is added to procs array for async execution.
// If config->async is false (default), waits for completion and returns success/failure immediately.
// Returns true if build succeeded or was skipped (up to date), false on failure.
// Automatically releases the command memory on completion. Creates output directory if needed.
QOLDEF bool qol_run_impl(QOL_Cmd *config, QOL_RunOptions opts);

// Always run a build command regardless of file modification times (unconditional build).
// Usage: qol_run_always(&cmd) or qol_run_always(&cmd, (QOL_RunOptions){ .procs = &procs }).
// If config->async is true and opts.procs is provided, process handle is added to procs array for async execution.
// If config->async is false (default), waits for completion and returns success/failure immediately.
// Returns true on success, false on failure. Automatically releases the command memory on completion.
// Creates output directory if needed. Useful for commands that should always run (e.g., tests, clean).
QOLDEF bool qol_run_always_impl(QOL_Cmd* config, QOL_RunOptions opts);

// Macros to make options parameter optional with designated initializer syntax.
// Usage: qol_run(&cmd) or qol_run(&cmd, .procs=&procs) - options are optional and can be specified by name.
// These macros wrap the _impl functions and allow convenient calling without always specifying options.
#define qol_run(cmd, ...) qol_run_impl(cmd, (QOL_RunOptions){__VA_ARGS__})
#define qol_run_always(cmd, ...) qol_run_always_impl(cmd, (QOL_RunOptions){__VA_ARGS__})

// Wait for an async process to complete and check its exit status.
// proc: Process handle returned from an async command execution (when config->async was true).
// Returns true if process exited successfully (exit code 0), false on failure or error.
// Blocks until the process completes. On Windows, closes the process handle after waiting.
// Logs error messages if the process failed or if waiting encounters an error.
QOLDEF bool qol_proc_wait(QOL_Proc proc);

// Wait for all processes in a Procs array to complete and check their exit statuses.
// procs: Pointer to QOL_Procs array containing process handles from async command executions.
// Returns true if all processes exited successfully, false if any process failed.
// Waits for each process sequentially and clears the procs array after completion.
// Useful for waiting on multiple parallel builds or commands executed asynchronously.
QOLDEF bool qol_procs_wait(QOL_Procs *procs);

// Automatically rebuild the current executable if source file is newer than the binary.
// src: Path to the source file of the current build system (e.g., "build.c").
// Checks modification time of src against the executable. If src is newer, rebuilds and restarts.
// On Unix, uses execv() to replace the current process. On Windows, spawns new process and exits.
// This enables automatic rebuild-on-change functionality for build scripts.
// If rebuild fails or restart fails, logs error and exits. If up to date, continues execution.
QOLDEF void qol_auto_rebuild(const char *src);

// Automatically rebuild the current executable if source file or any dependency is newer than the binary.
// src: Path to the source file of the current build system (e.g., "build.c").
// ...: Variadic list of dependency file paths (e.g., "build.h", "config.h"). Must end with NULL.
// Checks modification times of src and all dependencies against the executable.
// If any file is newer, rebuilds and restarts. Otherwise continues execution.
// More comprehensive than qol_auto_rebuild() as it checks multiple dependencies.
// The macro qol_auto_rebuild_plus() automatically appends NULL to the variadic arguments.
QOLDEF void qol_auto_rebuild_plus_impl(const char *src, ...);

// Macro to automatically append NULL to variadic args for qol_auto_rebuild_plus_impl().
// Usage: qol_auto_rebuild_plus("build.c", "build.h", "config.h") - NULL is appended automatically.
// Convenience wrapper that ensures the variadic argument list is properly terminated.
#define qol_auto_rebuild_plus(src, ...) qol_auto_rebuild_plus_impl(src, __VA_ARGS__, NULL)

// Extract the filename from a path without its extension. Returns a newly allocated string.
// path: Full file path (e.g., "/path/to/file.c" or "C:\\path\\file.c").
// Returns pointer to allocated string containing filename without extension (e.g., "file").
// Caller must free the returned string. Returns NULL on allocation failure.
// Handles both Unix '/' and Windows '\' path separators. Useful for auto-generating output names.
// TODO: Should be in @FILE_OPS, not in @NO_BUILD
QOLDEF char *qol_get_filename_no_ext(const char *path);

//////////////////////////////////////////////////
/// FILE_OPS /////////////////////////////////////
//////////////////////////////////////////////////

// String array structure: Dynamic array of strings (char*), used for reading files line-by-line
// Each element is a dynamically allocated string (one line from a file)
// Uses the same dynamic array pattern as other QOL structures (data, len, cap)
// Memory management: Each string element must be freed individually, then use qol_release_string()
// Example usage: Read file into QOL_String, process lines, then qol_release_string() to free all
typedef struct {
    char **data;  // Array of string pointers (each points to a line from a file)
    size_t len;    // Number of strings/lines currently stored
    size_t cap;    // Capacity of the data array (for dynamic growth)
} QOL_String;

// Create a directory at the specified path. Returns true on success, false on failure.
// On Unix-like systems, creates directory with permissions 0755.
// Logs an error message if the directory creation fails.
// SECURITY NOTE: Paths are used directly without validation. Only use trusted paths from your application.
// Paths containing ".." or absolute paths could access unintended directories.
QOLDEF bool qol_mkdir(const char *path);

// Create a directory at the specified path only if it doesn't already exist.
// Returns true if the directory exists or was successfully created, false on failure.
// This is a safe wrapper around qol_mkdir that checks for existence first.
QOLDEF bool qol_mkdir_if_not_exists(const char *path);

// Copy a file from src_path to dst_path. Returns true on success, false on failure.
// Creates the destination file if it doesn't exist, overwrites if it does.
// Uses a 4KB buffer for efficient copying. Logs errors if file operations fail.
// SECURITY NOTE: Paths are used directly without validation. Only use trusted paths from your application.
// Paths containing ".." could enable path traversal attacks.
QOLDEF bool qol_copy_file(const char *src_path, const char *dst_path);

// Recursively copy a directory and all its contents from src_path to dst_path.
// Returns true on success, false on failure. Creates destination directory if needed.
// Handles both files and subdirectories recursively. Skips "." and ".." entries.
QOLDEF bool qol_copy_dir_rec(const char *src_path, const char *dst_path);

// Read the contents of a directory and store full paths (parent/entry_name) in a QOL_String dynamic array.
// Returns true on success, false on failure. The content parameter must be initialized (or zeroed).
// Caller must free with qol_release_string. Skips "." and ".." entries.
QOLDEF bool qol_read_dir(const char *parent, QOL_String *content);

// Recursively read the contents of a directory and all subdirectories, storing full paths in a QOL_String dynamic array.
// Returns true on success, false on failure. The content parameter must be initialized (or zero
// ed). Caller must free with qol_release_string. Skips "." and ".." entries.
QOLDEF bool qol_read_dir_recursive(const char *parent, QOL_String *content);

// Read a file line by line into a QOL_String dynamic array. Each line becomes an element.
// Returns true on success, false on failure. Strips trailing newlines from each line.
// The content parameter must be initialized (or zeroed). Caller must free with qol_release_string.
QOLDEF bool qol_read_file(const char *path, QOL_String* content);

// Write binary data to a file. Creates the file if it doesn't exist, overwrites if it does.
// Returns true if all bytes were written successfully, false on failure.
// Logs an error if the file cannot be opened or if not all data was written.
QOLDEF bool qol_write_file(const char *path, const void *data, size_t size);

// Get the file extension from a file path. Returns pointer to extension (without dot),
// or "no_ext" if no extension found, or "unknown" if path is NULL.
// Example: "file.txt" returns "txt", "file" returns "no_ext".
QOLDEF const char *qol_get_file_type(const char *path);

// Delete a file at the specified path. Returns true on success, false on failure.
// Logs an error message if the file deletion fails. Works on both Unix and Windows.
QOLDEF bool qol_delete_file(const char *path);

// Recursively delete a directory and all its contents. Returns true on success, false on failure.
// Deletes all files and subdirectories recursively before removing the directory itself.
// Skips "." and ".." entries. Logs errors if deletion fails at any step.
QOLDEF bool qol_delete_dir(const char *path);

// Free all memory associated with a QOL_String dynamic array. Frees each string element
// and the array itself. Sets data to NULL and resets len and cap to 0.
// Safe to call on NULL or already-freed content.
QOLDEF void qol_release_string(QOL_String* content);

// Get all Files of a Directory and store them in a QOL_String array, returns true on success, false on failure.
QOLDEF bool qol_get_files_in_dir(const char *dir_path, QOL_String *files);

//////////////////////////////////////////////////
/// STRING UTILITIES /////////////////////////////
//////////////////////////////////////////////////

// Check if a string starts with a given prefix. Returns true if str starts with prefix, false otherwise.
// Both parameters must be non-NULL. Returns false if prefix is longer than str.
QOLDEF bool qol_str_starts_with(const char *str, const char *prefix);

// Check if a string ends with a given suffix. Returns true if str ends with suffix, false otherwise.
// Both parameters must be non-NULL. Returns false if suffix is longer than str.
QOLDEF bool qol_str_ends_with(const char *str, const char *suffix);

// Trim whitespace from both ends of a string in-place. Modifies the string directly.
// Returns a pointer to the trimmed string (may be different from input if leading whitespace was removed).
// The string is modified in-place, so ensure it's writable. Trims spaces, tabs, newlines, carriage returns.
QOLDEF char *qol_str_trim(char *str);

// Trim whitespace from the left (beginning) of a string in-place. Modifies the string directly.
// Returns a pointer to the trimmed string (may be different from input if leading whitespace was removed).
QOLDEF char *qol_str_ltrim(char *str);

// Trim whitespace from the right (end) of a string in-place. Modifies the string directly.
// Returns the same pointer as input (string is modified in-place).
QOLDEF char *qol_str_rtrim(char *str);

// Replace all occurrences of a substring in a string. Returns a newly allocated string with replacements.
// Returns NULL on allocation failure. Caller must free the returned string.
// This is a safe, non-regex replacement. If old_sub is empty or not found, returns a copy of str.
QOLDEF char *qol_str_replace(const char *str, const char *old_sub, const char *new_sub);

// Split a string by a delimiter character into a QOL_String array. Returns true on success, false on failure.
// The result parameter must be initialized (or zeroed). Each element in result will be a dynamically allocated string.
// Caller must free with qol_release_string(). Empty strings between consecutive delimiters are included as empty strings.
QOLDEF bool qol_str_split(const char *str, char delimiter, QOL_String *result);

// Join strings from a QOL_String array into a single string using a separator. Returns newly allocated string.
// Returns NULL on allocation failure. Caller must free the returned string.
// If the array is empty, returns an empty string (allocated). The separator is inserted between each string.
QOLDEF char *qol_str_join(QOL_String *strings, const char *separator);

// Check if a string contains a substring. Returns true if substring is found, false otherwise.
// Both parameters must be non-NULL. Returns false if substring is empty.
QOLDEF bool qol_str_contains(const char *str, const char *substring);

// Case-insensitive string comparison. Returns 0 if strings are equal (ignoring case), negative if str1 < str2, positive if str1 > str2.
// Similar to strcmp but case-insensitive. Both parameters must be non-NULL.
QOLDEF int qol_str_icmp(const char *str1, const char *str2);

// Path utilities

// Extract the filename (basename) from a file path. Returns pointer to the filename portion.
// Handles both Unix '/' and Windows '\' path separators. Returns the full path if no separator found.
// Example: "/path/to/file.txt" returns "file.txt", "C:\\path\\file.txt" returns "file.txt".
QOLDEF const char *qol_path_name(const char *path);

// Rename or move a file/directory from old_path to new_path. Returns true on success, false on failure.
// On Windows, replaces existing file if new_path exists. On Unix, overwrites if permissions allow.
// Logs an error message if the rename operation fails.
QOLDEF bool qol_rename(const char *old_path, const char *new_path);

// Get the current working directory as a string. Returns pointer to temp-allocated string.
// The returned string is allocated from the temporary allocator and will be valid until
// the next temp allocator reset. Returns NULL on failure. Logs errors if directory cannot be retrieved.
QOLDEF const char *qol_get_current_dir_temp(void);

// Change the current working directory to the specified path. Returns true on success, false on failure.
// Logs an error message if the directory change fails. Affects all subsequent relative path operations.
QOLDEF bool qol_set_current_dir(const char *path);

// Check if a file or directory exists at the specified path. Returns true if exists, false if not found.
// On error, logs an error message and returns false. Useful for checking file existence before operations.
// Changed return type from int (1/0/-1) to bool for consistency with other file operations.
QOLDEF bool qol_file_exists(const char *file_path);

// Rebuild detection

// Check if output_path needs to be rebuilt based on modification times of input files.
// Returns 1 if rebuild needed (any input is newer than output or output doesn't exist),
// 0 if up to date, -1 on error. Compares modification times of all input_paths against output_path.
// Logs errors if file operations fail. Used by build system to determine if compilation is needed.
QOLDEF int qol_needs_rebuild(const char *output_path, const char **input_paths, size_t input_paths_count);

// Convenience wrapper for qol_needs_rebuild with a single input file.
// Check if output_path needs to be rebuilt based on modification time of input_path.
// Returns 1 if rebuild needed, 0 if up to date, -1 on error.
QOLDEF int qol_needs_rebuild1(const char *output_path, const char *input_path);

//////////////////////////////////////////////////
/// TEMP_ALLOCATOR ///////////////////////////////
//////////////////////////////////////////////////

// Temporary allocator buffer size: 8MB default capacity
// Can be overridden by defining QOL_TEMP_CAPACITY before including this header
#ifndef QOL_TEMP_CAPACITY
    #define QOL_TEMP_CAPACITY (8*1024*1024)
#endif

// Fixed buffer sizes for command building and path operations
#define QOL_EXEC_BUFFER_SIZE 4096      // Maximum command line length
#define QOL_PATH_BUFFER_SIZE 1024     // Maximum path length for file operations
#define QOL_WIN32_ERR_BUFFER_SIZE (4*1024)  // Windows error message buffer size

// Temporary allocator: Fast, stack-like memory allocation that doesn't require manual freeing.
// All allocations are automatically freed when qol_temp_reset() is called.
// Useful for temporary strings and buffers that don't need to persist beyond a function scope.

// Duplicate a C string using temporary allocator. Returns pointer to temp-allocated copy.
// The returned string is valid until the next temp allocator reset. Returns NULL if allocation fails.
// Memory is allocated from a fixed-size buffer (QOL_TEMP_CAPACITY bytes).
QOLDEF char *qol_temp_strdup(const char *cstr);

// Allocate memory from the temporary allocator. Returns pointer to allocated memory, NULL on failure.
// Memory is aligned and allocated from a fixed-size buffer. No manual freeing required.
// All allocations are automatically freed when qol_temp_reset() is called.
// Returns NULL if requested size exceeds available capacity.
QOLDEF void *qol_temp_alloc(size_t size);

// Format a string using temporary allocator (like sprintf but returns temp-allocated string).
// Returns pointer to formatted string allocated from temp allocator, NULL on failure.
// The returned string is valid until the next temp allocator reset.
// Uses vsnprintf internally to determine required size, then allocates and formats.
QOLDEF char *qol_temp_sprintf(const char *format, ...);

// Reset the temporary allocator, freeing all previously allocated memory.
// After calling this, all pointers returned by temp allocator functions become invalid.
// Resets the internal allocation counter to zero, making all memory available again.
QOLDEF void qol_temp_reset(void);

// Save the current state of the temporary allocator. Returns a checkpoint value.
// Can be used with qol_temp_rewind() to restore the allocator to this point.
// Useful for implementing scoped temporary allocations within a larger scope.
size_t qol_temp_save(void);

// Rewind the temporary allocator to a previously saved checkpoint.
// All allocations made after the checkpoint are effectively freed.
// The checkpoint value should be obtained from qol_temp_save().
QOLDEF void qol_temp_rewind(size_t checkpoint);

// Windows error handling

#ifdef WINDOWS
    // Convert a Windows error code to a human-readable error message string.
    // Returns a pointer to a static buffer containing the formatted error message.
    // The message is formatted using FormatMessageA and has trailing whitespace removed.
    // Returns NULL only if formatting fails completely. Useful for error reporting.
    QOLDEF char *qol_win32_error_message(DWORD err);
#endif

//////////////////////////////////////////////////
/// AUTO_FREE ////////////////////////////////////
//////////////////////////////////////////////////

// Auto-free functionality: Automatically free heap-allocated memory when variables go out of scope.
// Uses GCC/Clang's __attribute__((cleanup)) extension to call a cleanup function automatically.
// This provides RAII-like behavior in C, reducing memory leaks by ensuring cleanup happens even on early returns.
//
// Usage:
//   QOL_AUTO_FREE void *ptr = malloc(100);
//   QOL_AUTO_FREE int *data = malloc(sizeof(int));
//   // ptr and data are automatically freed when they go out of scope
//
// Note: Only works with GCC and Clang compilers. On other compilers, QOL_AUTO_FREE is defined as empty.
// The cleanup function checks for NULL before freeing, so it's safe to use with uninitialized pointers.
//
// Implementation note: Uses a generic cleanup function. For typed pointers (like int*), you may need to
// cast to void* or use -Wno-incompatible-pointer-types. Alternatively, declare variables as void* and cast when needed.

#if defined(__GNUC__) || defined(__clang__)
    // Forward declaration - implementation is in QOL_IMPLEMENTATION section
    static inline void _qol_auto_free_impl(void *p);

    // Auto-free macro: Applies cleanup attribute
    // Note: For best compatibility, use void* for variables. For typed pointers, GCC may require
    // -Wno-incompatible-pointer-types flag or casting the variable to void*.
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wincompatible-pointer-types"
    #define QOL_AUTO_FREE __attribute__((cleanup(_qol_auto_free_impl)))
    #pragma GCC diagnostic pop
#else
    #define QOL_AUTO_FREE  // No-op on compilers without cleanup attribute support
#endif

//////////////////////////////////////////////////
/// DYN_ARRAY ////////////////////////////////////
//////////////////////////////////////////////////

// Dynamic array implementation: Provides automatic memory management for arrays
// All macros operate on structures with: T *data, size_t len, size_t cap fields
// Pattern: Initialize to zero, use qol_push() to add elements, qol_release() to free
// The array automatically grows when capacity is exceeded (doubles each time)

// Initial capacity for new dynamic arrays (allocated on first push)
#define QOL_INIT_CAP 8

// Grow macro: Ensure the dynamic array has capacity for at least `n` elements
// If current capacity is less than `n`, reallocates to a larger size
// Growth strategy: Start at QOL_INIT_CAP, double capacity each time until >= n
// This exponential growth ensures O(1) amortized time per insertion
// Logs debug messages when memory is allocated or reallocated
// Aborts on allocation failure (out of memory) - this is a fatal error
// Usage: qol_grow(&vec, vec.len + 1) before adding an element
#define qol_grow(vec, n)                                                                                     \
    do {                                                                                                     \
        if ((n) > (vec)->cap) {                                                                              \
            /* Calculate new capacity: start at INIT_CAP if empty, otherwise double until >= n */             \
            size_t newcap = (vec)->cap ? (vec)->cap : QOL_INIT_CAP;                                          \
            while (newcap < (n)) newcap *= 2;                                                                \
            /* Log allocation event for debugging */                                                          \
            if ((vec)->cap == 0) {                                                                           \
                qol_log(QOL_LOG_DIAG, "Dynamic array inits memory on %zu.\n", newcap);                      \
            } else {                                                                                         \
                qol_log(QOL_LOG_DIAG, "Dynamic array needs more memory (%zu -> %zu)!\n", (vec)->cap, newcap); \
            }                                                                                                \
            /* Reallocate memory - realloc handles NULL pointer (first allocation) */                        \
            void *tmp = realloc((vec)->data, newcap * sizeof(*(vec)->data));                                 \
            if (!tmp) {                                                                                      \
                qol_log(QOL_LOG_ERRO, "Dynamic array out of memory (need %zu elements)\n", (size_t)(n));    \
                abort();                                                                                     \
            }                                                                                                \
            (vec)->data = tmp;                                                                               \
            (vec)->cap = newcap;                                                                             \
        }                                                                                                    \
    } while (0)

// Shrink macro: Reduce array capacity if it's less than half full (to save memory)
// Only shrinks if length < capacity/2 AND capacity > QOL_INIT_CAP (never shrink below initial size)
// This prevents thrashing if elements are repeatedly added/removed near the threshold
// Uses realloc to reduce memory usage. If realloc fails, keeps old capacity (non-fatal)
// Logs debug message when shrinking occurs
#define qol_shrink(vec)                                                                                        \
    do {                                                                                                       \
        if ((vec)->len < (vec)->cap / 2 && (vec)->cap > QOL_INIT_CAP) {                                        \
            size_t newcap = (vec)->cap / 2;                                                                    \
            qol_log(QOL_LOG_DIAG, "Dynamic array can release some memory (%zu -> %zu)!\n", (vec)->cap, newcap); \
            void *tmp = realloc((vec)->data, newcap * sizeof(*(vec)->data));                                   \
            if (tmp) {                                                                                         \
                (vec)->data = tmp;                                                                             \
                (vec)->cap = newcap;                                                                           \
            }                                                                                                  \
        }                                                                                                      \
    } while (0)

// Internal push implementation: Add a single value to the end of the array
// Grows array if needed, then assigns value and increments length
// This is the core operation - variadic qol_push() calls this in a loop
#define qol_push_impl(vec, val)            \
    do {                                   \
        qol_grow((vec), (vec)->len+1);     \
        (vec)->data[(vec)->len++] = (val); \
    } while (0)

// Variadic push macro: Add one or more values to the end of the array
// Usage: qol_push(&vec, val) or qol_push(&vec, a, b, c, ...) - supports any number of arguments
// Implementation trick: Creates a temporary array from variadic args, then loops to push each
// Uses typeof() GCC/Clang extension for automatic type inference (no need to specify type)
// The double-underscore prefix (__vec, __temp, etc.) avoids name collisions with user code
// Example: qol_push(&cmd, "cc", "-Wall", "main.c", "-o", "main") adds 5 arguments
#define qol_push(vec, ...) \
    do { \
        typeof(*vec) *__vec = (vec); \
        typeof(__vec->data[0]) __temp[] = {__VA_ARGS__}; \
        size_t __count = sizeof(__temp) / sizeof(__temp[0]); \
        for (size_t __i = 0; __i < __count; __i++) { \
            qol_push_impl(__vec, __temp[__i]); \
        } \
    } while (0)


// Drop macro: Remove and discard the last element from the array (like pop_back)
// Decrements length and may shrink capacity if array becomes less than half full
// Aborts if called on empty array (undefined behavior prevention)
// Usage: qol_drop(&vec); // Removes last element
#define qol_drop(vec)                                              \
    do {                                                           \
        if ((vec)->len == 0) {                                     \
            qol_log(QOL_LOG_ERRO, "qol_drop() on empty array\n"); \
            abort();                                               \
        }                                                          \
        --(vec)->len;                                              \
        qol_shrink(vec);                                           \
    } while (0)

// Drop at index macro: Remove element at index n, shifting remaining elements down
// This is O(n) operation as it must move all elements after index n
// Uses memmove() for safe overlapping memory copy
// Aborts if index is out of range
// Usage: qol_dropn(&vec, 2); // Removes element at index 2
#define qol_dropn(vec, n)                                                \
    do {                                                                 \
        size_t __idx = (n);                                              \
        if (__idx >= (vec)->len) {                                       \
            qol_log(QOL_LOG_ERRO, "qol_dropn(): index out of range\n"); \
            abort();                                                     \
        }                                                                \
        /* Shift elements after index down by one position */            \
        memmove((vec)->data + __idx,                                     \
                (vec)->data + __idx + 1,                                 \
                ((vec)->len - __idx - 1) * sizeof(*(vec)->data));        \
        --(vec)->len;                                                    \
        qol_shrink(vec);                                                 \
    } while (0)

// Resize macro: Set array length to exactly n elements
// Grows array if n > capacity, but does NOT initialize new elements (they contain garbage)
// If n < current length, effectively truncates the array (elements beyond n are lost)
// Usage: qol_resize(&vec, 10); // Set length to 10
#define qol_resize(vec, n)    \
    do {                      \
        qol_grow((vec), (n)); \
        (vec)->len = (n);     \
    } while (0)

// Release macro: Free all memory associated with the dynamic array
// Sets data to NULL and resets length and capacity to 0
// Safe to call multiple times (idempotent)
// Usage: qol_release(&vec); // Free memory, array is now empty
#define qol_release(vec)             \
    do {                             \
        free((vec)->data);           \
        (vec)->data = NULL;          \
        (vec)->len = (vec)->cap = 0; \
    } while (0)

// Back macro: Get the last element of the array (like back() in C++)
// Returns last element if array is non-empty, otherwise aborts with error message
// Usage: int last = qol_back(&vec); // Get last element
#define qol_back(vec) \
    ((vec)->len > 0 ? (vec)->data[(vec)->len-1] : \
     (fprintf(stderr, "[ERRO] qol_back() on empty array\n"), abort(), (vec)->data[0]))

// Swap macro: Swap element at index i with the last element (without removing)
// Useful for implementing remove-by-value: swap target to end, then drop
// Aborts if index is out of range
// Usage: qol_swap(&vec, 3); // Swap element 3 with last element
#define qol_swap(vec, i)                                          \
    do {                                                          \
        size_t __idx = (i);                                       \
        if (__idx >= (vec)->len) {                                \
            qol_log(QOL_LOG_ERRO, "qol_swap(): out of range\n"); \
            abort();                                              \
        }                                                         \
        typeof((vec)->data[0]) __tmp = (vec)->data[__idx];        \
        (vec)->data[__idx] = (vec)->data[(vec)->len - 1];         \
        (vec)->data[(vec)->len - 1] = __tmp;                      \
    } while (0)

// List macro: Create a dynamic array type definition
// Usage: qol_list(int) numbers; // Creates struct { int *data; size_t len, cap; } numbers;
// Convenience macro for declaring dynamic arrays without typing the full struct definition
#define qol_list(T) \
    struct { T *data; size_t len, cap; }

//////////////////////////////////////////////////
/// HASHMAP //////////////////////////////////////
//////////////////////////////////////////////////

// Hashmap implementation: Key-value store using open addressing with linear probing
// Keys are always strings (null-terminated). Values are stored as void* pointers.
// Uses djb2 hash function. Automatically resizes when load factor exceeds 0.75

// Entry state enumeration: Tracks the state of each hashmap bucket
// Used for open addressing collision resolution - distinguishes empty slots from deleted ones
typedef enum {
    QOL_HM_EMPTY = 0,    // Bucket is empty (never used or was cleared)
    QOL_HM_USED,         // Bucket contains a valid key-value pair
    QOL_HM_DELETED       // Bucket was used but entry was deleted (tombstone for probing)
} qol_hm_entry_state_t;

// Hashmap entry structure: Represents a single key-value pair in the hashmap
// Keys and values are stored as dynamically allocated memory (caller manages value lifetime)
// The state field indicates whether this entry is empty, used, or deleted (for linear probing)
typedef struct {
    void *key;                    // Pointer to allocated key string (null-terminated)
    void *value;                  // Pointer to value (stored as pointer to pointer for void* values)
    size_t key_size;              // Size of allocated key in bytes (including null terminator)
    size_t value_size;            // Size of value storage (always sizeof(void*) currently)
    qol_hm_entry_state_t state;   // State of this entry (EMPTY, USED, or DELETED)
} QOL_HashMapEntry;

// Hashmap structure: Main hashmap container with bucket array
// Uses open addressing with linear probing for collision resolution
// Automatically resizes when load factor (size/capacity) exceeds 0.75
typedef struct {
    QOL_HashMapEntry *buckets;  // Array of hashmap entries (the hash table)
    size_t capacity;             // Total number of buckets (always power of 2, minimum 4)
    size_t size;                 // Number of key-value pairs currently stored
} QOL_HashMap;

// Create an empty hashmap with initial capacity of 4 buckets. Returns pointer to new hashmap, NULL on failure.
// The hashmap uses open addressing with linear probing for collision resolution.
// Automatically resizes when load factor exceeds 0.75. Caller must free with qol_hm_release().
QOLDEF QOL_HashMap *qol_hm_create();

// Insert or update a key-value pair in the hashmap. Keys are strings (null-terminated), values are void* pointers.
// If key already exists, updates the existing value. If key doesn't exist, creates new entry.
// Keys are copied internally, but values are stored as pointers (caller manages value lifetime).
// IMPORTANT: The hashmap stores only the pointer to the value, not the value itself. The caller must ensure
// that the value pointer remains valid for the lifetime of the hashmap entry. Do not free the value or
// use stack-allocated values unless you guarantee they remain valid. Values are freed when the entry is
// removed or the hashmap is cleared/released, but only the pointer storage is freed, not the data it points to.
// The hashmap automatically resizes if load factor exceeds 0.75 after insertion.
QOLDEF void qol_hm_put(QOL_HashMap *hm, void *key, void *value);

// Retrieve a value from the hashmap by key. Returns pointer to value if found, NULL if key doesn't exist.
// Keys are compared as null-terminated strings. Returns NULL if hashmap is NULL or key is NULL.
// The returned pointer is the same pointer that was stored with qol_hm_put(). The caller must not free
// this pointer - it is managed by the hashmap. The pointer remains valid until the entry is removed
// or the hashmap is cleared/released. However, the data the pointer points to must remain valid (see qol_hm_put).
QOLDEF void *qol_hm_get(QOL_HashMap *hm, void *key);

// Check if the hashmap contains a specific key. Returns true if key exists, false otherwise.
// More efficient than qol_hm_get() when you only need to check existence (doesn't retrieve value).
// Returns false if hashmap is NULL or key is NULL.
QOLDEF bool qol_hm_contains(QOL_HashMap *hm, void *key);

// Remove a key-value pair from the hashmap by key. Returns true if key was found and removed, false otherwise.
// Uses tombstone marking (DELETED state) for proper linear probing after deletion.
// Frees the memory allocated for the key. Returns false if hashmap is NULL, key is NULL, or key not found.
QOLDEF bool qol_hm_remove(QOL_HashMap *hm, void *key);

// Remove all entries from the hashmap, freeing all keys but keeping the hashmap structure intact.
// The hashmap can be reused after clearing. Capacity remains unchanged (memory not freed).
// Safe to call on NULL hashmap. After clearing, size is 0 but capacity is unchanged.
QOLDEF void qol_hm_clear(QOL_HashMap* hm);

// Free all memory associated with the hashmap, including the hashmap structure itself.
// Calls qol_hm_clear() first to free all entries, then frees the bucket array and hashmap structure.
// Safe to call on NULL hashmap. After release, the hashmap pointer is invalid and should not be used.
QOLDEF void qol_hm_release(QOL_HashMap* hm);

// Get the number of key-value pairs currently stored in the hashmap. Returns 0 if hashmap is NULL.
// This is the actual number of entries, not the capacity (number of buckets).
QOLDEF size_t qol_hm_size(QOL_HashMap* hm);

// Check if the hashmap is empty (contains no entries). Returns true if empty or NULL, false otherwise.
// More efficient than checking qol_hm_size() == 0. Returns true if hashmap is NULL.
QOLDEF bool qol_hm_empty(QOL_HashMap* hm);

//////////////////////////////////////////////////
/// THREAD SAFETY ////////////////////////////////
//////////////////////////////////////////////////

// Thread-safety infrastructure: Cross-platform mutex support
// Uses pthread_mutex_t on Unix-like systems and CRITICAL_SECTION on Windows
// All global state access is protected by these mutexes
#if defined(WINDOWS)
    typedef CRITICAL_SECTION QOL_Mutex;
    #define QOL_MUTEX_INIT(mutex) InitializeCriticalSection(&(mutex))
    #define QOL_MUTEX_LOCK(mutex) EnterCriticalSection(&(mutex))
    #define QOL_MUTEX_UNLOCK(mutex) LeaveCriticalSection(&(mutex))
    #define QOL_MUTEX_DESTROY(mutex) DeleteCriticalSection(&(mutex))
#else
    typedef pthread_mutex_t QOL_Mutex;
    #define QOL_MUTEX_INIT(mutex) pthread_mutex_init(&(mutex), NULL)
    #define QOL_MUTEX_LOCK(mutex) pthread_mutex_lock(&(mutex))
    #define QOL_MUTEX_UNLOCK(mutex) pthread_mutex_unlock(&(mutex))
    #define QOL_MUTEX_DESTROY(mutex) pthread_mutex_destroy(&(mutex))
#endif

// Initialize all mutexes (called automatically on first use, thread-safe).
// Declared here because macros like QOL_TEST_ASSERT expand in user code and call it.
QOLDEF void qol_init_mutexes(void);

// Mutexes for protecting global state
#if defined(WINDOWS)
    // On Windows, CRITICAL_SECTION must be initialized dynamically
    static CRITICAL_SECTION qol_logger_mutex;
    static CRITICAL_SECTION qol_temp_alloc_mutex;
    static CRITICAL_SECTION qol_argparser_mutex;
    static CRITICAL_SECTION qol_test_mutex;
    static CRITICAL_SECTION qol_win32_err_mutex;
    static volatile LONG qol_mutexes_initialized = 0;  // 0=uninit, 1=initting, 2=done
#else
    // On Unix, use PTHREAD_MUTEX_INITIALIZER for static initialization
    static pthread_mutex_t qol_logger_mutex = PTHREAD_MUTEX_INITIALIZER;
    static pthread_mutex_t qol_temp_alloc_mutex = PTHREAD_MUTEX_INITIALIZER;
    static pthread_mutex_t qol_argparser_mutex = PTHREAD_MUTEX_INITIALIZER;
    static pthread_mutex_t qol_test_mutex = PTHREAD_MUTEX_INITIALIZER;
    static pthread_mutex_t qol_win32_err_mutex = PTHREAD_MUTEX_INITIALIZER;
    static volatile int qol_mutexes_initialized = 1;  // Already initialized on Unix
#endif

//////////////////////////////////////////////////
/// HELPER ///////////////////////////////////////
//////////////////////////////////////////////////

// Helper macros: Common utilities for C programming

// Unused macro: Suppress compiler warnings for unused variables/parameters
// Usage: QOL_UNUSED(unused_param); // Tells compiler this variable is intentionally unused
#define QOL_UNUSED(value) (void)(value)

// TODO macro: Mark code locations that need implementation
// When executed, prints file:line and message, then exits with failure
// Usage: QOL_TODO("Implement feature X"); // Marks incomplete code
#define QOL_TODO(message) do { fprintf(stderr, "%s:%d: TODO: %s\n", __FILE__, __LINE__, message); exit(EXIT_FAILURE); } while(0)

// Unreachable macro: Mark code that should never be executed (for error handling)
// When executed, prints file:line and message, then aborts
// Usage: QOL_UNREACHABLE("This should never happen"); // Marks impossible code paths
#define QOL_UNREACHABLE(message) do { fprintf(stderr, "%s:%d: UNREACHABLE: %s\n", __FILE__, __LINE__, message); abort(); } while(0)

// Array length macro: Calculate number of elements in a statically-sized array
// Works at compile-time for arrays declared with [] syntax
// Usage: int arr[10]; size_t len = QOL_ARRAY_LEN(arr); // Returns 10
#define QOL_ARRAY_LEN(array) (sizeof(array)/sizeof(array[0]))

// Array get macro: Safely access array element with bounds checking
// Asserts that index is within bounds before accessing
// Usage: int val = QOL_ARRAY_GET(arr, 5); // Gets arr[5] with bounds check
#define QOL_ARRAY_GET(array, index) \
    (QOL_ASSERT((size_t)(index) < QOL_ARRAY_LEN(array)), (array)[(size_t)(index)])

//////////////////////////////////////////////////
/// UNITTEST /////////////////////////////////////
//////////////////////////////////////////////////

// Unit test framework: Simple test registration and execution system
// Tests are automatically registered via constructor attributes (GCC/Clang extension)
// Use QOL_TEST macro to define tests - they run when qol_test_run_all() is called

// Test structure: Represents a single unit test function
// Tests are registered automatically when the program starts (via constructor attribute)
typedef struct {
    void (*func)(void);      // Pointer to test function (takes no args, returns void)
    const char *name;        // Test name (for display in results)
    const char *file;        // Source file where test is defined (for error reporting)
    int line;                // Line number where test is defined (for error reporting)
} qol_test_t;

// Register a test function with the test framework. Called automatically by QOL_TEST macro.
// Stores test name, source file, line number, and function pointer for later execution.
// Tests are typically registered via the QOL_TEST macro using constructor attributes.
QOLDEF void qol_test_register(const char *name, const char *file, int line, void (*test_func)(void));

// Mark the current test as failed. Should be called from within a test function.
// Sets an internal flag that will cause the test to be reported as failed when it completes.
// Usually called indirectly through assertion macros like QOL_TEST_ASSERT.
QOLDEF void qol_test_fail(void);

// Run all registered tests and return exit code. Returns 0 if all tests passed, 1 if any failed.
// Executes each registered test function and reports pass/fail status with colored output.
// Prints a summary of total tests, passed tests, and failed tests at the end.
QOLDEF int qol_test_run_all(void);

// Print a summary of test results. Shows total tests, passed count, and failed count.
// Should be called after qol_test_run_all() to display final statistics.
QOLDEF void qol_test_print_summary(void);

// Internal failure message storage
extern char qol_test_failure_msg[];

// Test assertion macros: Check conditions and fail test if assertion fails

// Base assertion macro: Check a condition, fail test with message if false
// Stores failure message in global buffer and marks test as failed
// Returns from test function immediately if assertion fails
// Usage: QOL_TEST_ASSERT(x > 0, "x must be positive");
#define QOL_TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            qol_init_mutexes(); \
            QOL_MUTEX_LOCK(qol_test_mutex); \
            snprintf(qol_test_failure_msg, sizeof(qol_test_failure_msg), "%s:%d: %s", __FILE__, __LINE__, message); \
            QOL_MUTEX_UNLOCK(qol_test_mutex); \
            qol_test_fail(); \
            return; \
        } \
    } while(0)

// Equality assertion: Check if two values are equal
// Usage: QOL_TEST_EQ(actual, expected, "values should match");
#define QOL_TEST_EQ(a, b, message) QOL_TEST_ASSERT((a) == (b), message)

// Inequality assertion: Check if two values are not equal
// Usage: QOL_TEST_NEQ(ptr, NULL, "pointer should not be null");
#define QOL_TEST_NEQ(a, b, message) QOL_TEST_ASSERT((a) != (b), message)

// String equality assertion: Check if two strings are equal (case-sensitive)
// Usage: QOL_TEST_STREQ(str, "expected", "string should match");
#define QOL_TEST_STREQ(a, b, message) QOL_TEST_ASSERT(strcmp((a), (b)) == 0, message)

// String inequality assertion: Check if two strings are not equal
// Usage: QOL_TEST_STRNEQ(str1, str2, "strings should differ");
#define QOL_TEST_STRNEQ(a, b, message) QOL_TEST_ASSERT(strcmp((a), (b)) != 0, message)

// Truthy assertion: Check if value is non-zero (true)
// Usage: QOL_TEST_TRUTHY(result, "operation should succeed");
#define QOL_TEST_TRUTHY(value, message) QOL_TEST_ASSERT(value, message)

// Falsy assertion: Check if value is zero (false)
// Usage: QOL_TEST_FALSY(error, "operation should not fail");
#define QOL_TEST_FALSY(value, message) QOL_TEST_ASSERT(!(value), message)

// Test definition macro: Define a unit test function that auto-registers itself
// Usage: QOL_TEST(my_test) { QOL_TEST_EQ(1+1, 2, "math should work"); }
// This macro:
// 1. Declares the test function (qol_test_##name)
// 2. Creates a constructor function that registers the test at program startup
// 3. Defines the test function body (user provides body after macro)
// The constructor attribute (GCC/Clang) ensures registration happens before main()
#define QOL_TEST(name) \
    static void qol_test_##name(void); \
    __attribute__((constructor)) static void qol_test_register_##name(void) { \
        qol_test_register(#name, __FILE__, __LINE__, qol_test_##name); \
    } \
    static void qol_test_##name(void)

//////////////////////////////////////////////////
/// TIMER ////////////////////////////////////////
//////////////////////////////////////////////////

// High-resolution timer structure: Platform-agnostic timing for performance measurements
// Provides nanosecond-precision timing using platform-specific high-resolution clocks
// On Windows: Uses QueryPerformanceCounter (typically microsecond precision, can be nanosecond)
// On Unix: Uses clock_gettime(CLOCK_MONOTONIC) (nanosecond precision, monotonic clock)
// Monotonic clock means it's not affected by system clock adjustments (NTP, manual changes)
typedef struct {
#if defined(WINDOWS)
    LARGE_INTEGER start;      // Start time in performance counter ticks
    LARGE_INTEGER frequency;   // Performance counter frequency (ticks per second) for conversion
#else
    struct timespec start;     // Start time: tv_sec (seconds) + tv_nsec (nanoseconds)
#endif
} QOL_Timer;

// Start a high-resolution timer. Records the current time for later elapsed time calculations.
// Uses QueryPerformanceCounter on Windows and clock_gettime(CLOCK_MONOTONIC) on Unix.
// Timer must be initialized before calling elapsed time functions. Safe to call with NULL.
QOLDEF void qol_timer_start(QOL_Timer *timer);

// Get elapsed time since timer was started, in seconds as a double precision float.
// Returns 0.0 if timer is NULL. Uses high-resolution timing suitable for performance measurements.
// On Windows uses QueryPerformanceCounter, on Unix uses clock_gettime(CLOCK_MONOTONIC).
QOLDEF double qol_timer_elapsed(QOL_Timer *timer);

// Get elapsed time since timer was started, in milliseconds as a double precision float.
// Returns elapsed time converted to milliseconds (seconds * 1000.0).
// Useful for timing operations that take milliseconds to complete.
QOLDEF double qol_timer_elapsed_ms(QOL_Timer *timer);

// Get elapsed time since timer was started, in microseconds as a double precision float.
// Returns elapsed time converted to microseconds (seconds * 1000000.0).
// Useful for precise timing measurements of fast operations.
QOLDEF double qol_timer_elapsed_us(QOL_Timer *timer);

// Get elapsed time since timer was started, in nanoseconds as a 64-bit unsigned integer.
// Returns elapsed time converted to nanoseconds. Most precise timing available.
// Useful for very high-resolution timing requirements. Returns 0 if timer is NULL.
QOLDEF uint64_t qol_timer_elapsed_ns(QOL_Timer *timer);

// Reset timer to current time, effectively restarting the elapsed time measurement.
// Equivalent to calling qol_timer_start() again. Safe to call with NULL.
QOLDEF void qol_timer_reset(QOL_Timer *timer);

//////////////////////////////////////////////////
/// QOL_IMPLEMENATION ////////////////////////////
//////////////////////////////////////////////////

#ifdef QOL_IMPLEMENTATION

    //////////////////////////////////////////////////
    /// THREAD SAFETY ////////////////////////////////
    //////////////////////////////////////////////////

    // Initialize all mutexes (called automatically on first use, thread-safe)
    QOLDEF void qol_init_mutexes(void) {
#if defined(WINDOWS)
        // Simple spin-lock for initialization (Windows only, since CRITICAL_SECTION needs init)
        LONG expected = 0;
        if (InterlockedCompareExchange(&qol_mutexes_initialized, 1, 0) == 0) {
            // We're the first thread to initialize
            InitializeCriticalSection(&qol_logger_mutex);
            InitializeCriticalSection(&qol_temp_alloc_mutex);
            InitializeCriticalSection(&qol_argparser_mutex);
            InitializeCriticalSection(&qol_test_mutex);
            InitializeCriticalSection(&qol_win32_err_mutex);
            InterlockedExchange(&qol_mutexes_initialized, 2);  // Mark as fully initialized
        } else {
            // Wait for initialization to complete (spin-wait, should be very fast)
            while (qol_mutexes_initialized != 2) {
                Sleep(0);  // Yield to other threads
            }
        }
#else
        // On Unix, mutexes are statically initialized, nothing to do
        (void)0;  // Suppress unused function warning
#endif
    }

    // Thread-local storage for time/date buffers (better than mutex for these)
    // Each thread gets its own buffer, eliminating race conditions
#if defined(WINDOWS)
    __declspec(thread) static char qol_time_buf_tls[64] = {0};
    __declspec(thread) static char qol_date_buf_tls[64] = {0};
    __declspec(thread) static char qol_datetime_buf_tls[64] = {0};
    __declspec(thread) static bool qol_test_current_failed_tls = false;
#else
    static __thread char qol_time_buf_tls[64] = {0};
    static __thread char qol_date_buf_tls[64] = {0};
    static __thread char qol_datetime_buf_tls[64] = {0};
    static __thread bool qol_test_current_failed_tls = false;
#endif

    //////////////////////////////////////////////////
    /// ANSI COLORS //////////////////////////////////
    //////////////////////////////////////////////////

    // Enable ANSI color codes on Windows console
    // Reference: https://github.com/mlabbe/ansicodes/blob/main/ansicodes.h#L305-L316
    // On Windows, ANSI escape sequences are disabled by default. This function enables them.
    // ENABLE_VIRTUAL_TERMINAL_PROCESSING: Allows ANSI escape sequences to work
    // DISABLE_NEWLINE_AUTO_RETURN: Prevents Windows from converting \n to \r\n automatically
    // On Unix-like systems, this is a no-op (ANSI codes work by default)
    QOLDEF void QOL_enable_ansi(void) {
#if defined(WINDOWS)
    #ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
        #define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
    #endif
    #ifndef DISABLE_NEWLINE_AUTO_RETURN
        #define DISABLE_NEWLINE_AUTO_RETURN 0x0008
    #endif
        // Enable virtual terminal processing on both stdout and stderr so that
        // ANSI escape sequences work in the Windows console (Windows 10+).
        HANDLE handles[2] = { GetStdHandle(STD_OUTPUT_HANDLE), GetStdHandle(STD_ERROR_HANDLE) };
        for (int i = 0; i < 2; i++) {
            DWORD mode = 0;
            if (handles[i] == INVALID_HANDLE_VALUE || handles[i] == NULL) continue;
            if (!GetConsoleMode(handles[i], &mode)) continue;  // Not a console (redirected)
            mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;        // Enable ANSI escape sequence support
            mode |= DISABLE_NEWLINE_AUTO_RETURN;               // Disable automatic \r insertion
            SetConsoleMode(handles[i], mode);
        }
#endif
    }

    //////////////////////////////////////////////////
    /// LOGGER ///////////////////////////////////////
    //////////////////////////////////////////////////

    // Color mappings for each log level: Maps log levels to ANSI color codes
    // These colors are applied to log level labels when color output is enabled
    #define QOL_COLOR_RESET     QOL_RESET                // Reset color (default)
    #define QOL_COLOR_INFO      QOL_FG_BBLACK            // Bright black (gray) for info
    #define QOL_COLOR_EXEC      QOL_FG_BCYAN             // Cyan for commands (distinctive)
    #define QOL_COLOR_DIAG      QOL_FG_GREEN             // Green for debug (less intrusive)
    #define QOL_COLOR_HINT      QOL_FG_BLUE              // Blue for hints (informational)
    #define QOL_COLOR_WARN      QOL_FG_YELLOW            // Yellow for warnings (attention)
    #define QOL_COLOR_ERRO      QOL_BOLD QOL_FG_RED      // Bold red for errors (critical)
    #define QOL_COLOR_DEAD      QOL_BOLD QOL_FG_MAGENTA  // Bold magenta for critical (fatal)

    // Logger state: Static variables that persist across logger function calls
    qol_log_level_t qol_logger_min_level = QOL_LOG_INFO;  // Minimum level to display (default: INFO)
    bool qol_logger_color = false;                        // Whether to use ANSI colors (default: off)
    bool qol_logger_time = true;                          // Whether to show timestamps (default: on)
    bool qol_logger_time_color = false;                   // Whether to show timestamps with color (default: on)
    FILE *qol_log_file = NULL;                            // Optional log file handle (NULL = no file logging)
    bool qol_logger_only_mode = false;                    // Only log messages at exactly only_level (default: off)
    qol_log_level_t qol_logger_only_level = QOL_LOG_DIAG; // Level to use when only_mode is enabled

    QOLDEF void qol_init_logger_impl(qol_init_logger_arguments args) {
        qol_init_mutexes();
        QOL_MUTEX_LOCK(qol_logger_mutex);
        qol_logger_min_level = args.level;
        qol_logger_color = args.color;
        qol_logger_time = args.time;
        qol_logger_time_color = args.time_color;
        qol_logger_only_mode = args.only_set;
        qol_logger_only_level = args.only;
        QOL_MUTEX_UNLOCK(qol_logger_mutex);
    }

    // TODO: should be moved to file utils?
    QOLDEF char *qol_expand_path(const char *path) {
        if (!path) return NULL;

        // Check if path starts with ~ (tilde expansion)
        if (path[0] == '~' && (path[1] == '/' || path[1] == '\0')) {
            const char *home = NULL;
            // Get home directory path from environment variable (platform-specific)
#if defined(MACOS) || defined(LINUX)
            home = getenv("HOME");           // Unix standard: $HOME
#elif defined(WINDOWS)
            home = getenv("USERPROFILE");    // Windows: %USERPROFILE%
            if (!home) home = getenv("HOMEPATH");  // Fallback: %HOMEPATH%
#endif
            if (!home) {
                fprintf(stderr, "Failed to get home directory\n");
                return strdup(path); // Return original path if home not found
            }

            // Allocate buffer: home directory + rest of path + null terminator
            size_t home_len = strlen(home);
            size_t path_len = strlen(path);
            char *expanded = (char *)malloc(home_len + path_len + 1);
            if (!expanded) return NULL;

            strcpy(expanded, home);  // Start with home directory
            if (path[1] == '/') {
                strcat(expanded, path + 1);  // Skip ~ and keep / (e.g., "~/file" -> "/home/user/file")
            } else if (path[1] != '\0') {
                strcat(expanded, path + 1);  // Skip ~ (e.g., "~file" -> "/home/userfile")
            }
            // If path is just "~", expanded is already home directory (no concatenation needed)
            return expanded;
        }

        // No tilde expansion needed, return copy of original path
        return strdup(path);
    }

    QOLDEF const char *qol_get_time(void) { // TODO: set the fmt as a parameter
        time_t t = time(NULL);
        struct tm *lt = localtime(&t);
        strftime(qol_time_buf_tls, sizeof(qol_time_buf_tls), "%H-%M-%S", lt);
        return qol_time_buf_tls;
    }

    QOLDEF const char *qol_get_date(void) { // TODO: set the fmt as a parameter
        time_t t = time(NULL);
        struct tm *lt = localtime(&t);
        strftime(qol_date_buf_tls, sizeof(qol_date_buf_tls), "%Y-%m-%d", lt);
        return qol_date_buf_tls;
    }

    QOLDEF const char *qol_get_datetime(void) { // TODO: set the fmt as a parameter
        time_t t = time(NULL);
        struct tm *lt = localtime(&t);
        strftime(qol_datetime_buf_tls, sizeof(qol_datetime_buf_tls), "%Y-%m-%d_%H-%M-%S", lt);
        return qol_datetime_buf_tls;
    }

    QOLDEF void qol_init_logger_logfile(const char *format, ...) {
        qol_init_mutexes();
        QOL_MUTEX_LOCK(qol_logger_mutex);
        // Close existing log file if open
        if (qol_log_file != NULL) {
            fclose(qol_log_file);
            qol_log_file = NULL;
        }

        // Open new log file if format is provided
        if (format != NULL) {
            char path[1024];

            va_list args;
            va_start(args, format);
            vsnprintf(path, sizeof(path), format, args);
            va_end(args);

            char *expanded_path = qol_expand_path(path);
            if (!expanded_path) {
                QOL_MUTEX_UNLOCK(qol_logger_mutex);
                fprintf(stderr, "Failed to expand path: %s\n", path);
                return;
            }

            qol_log_file = fopen(expanded_path, "a"); // Append mode
            if (qol_log_file == NULL) {
                fprintf(stderr, "Failed to open log file: %s\n", expanded_path);
            }

            free(expanded_path);
        }
        QOL_MUTEX_UNLOCK(qol_logger_mutex);
    }

    QOLDEF const char *qol_level_to_str(qol_log_level_t level) {
        switch (level) {
        case QOL_LOG_DIAG: return "DIAG";
        case QOL_LOG_INFO: return "INFO";
        case QOL_LOG_EXEC: return "EXEC";
        case QOL_LOG_HINT: return "HINT";
        case QOL_LOG_WARN: return "WARN";
        case QOL_LOG_ERRO: return "ERRO";
        case QOL_LOG_DEAD: return "DEAD";
        default:           return "UNKNOWN";
        }
    }

    QOLDEF const char *qol_level_to_color(qol_log_level_t level) {
        switch (level) {
        case QOL_LOG_DIAG: return QOL_COLOR_DIAG;
        case QOL_LOG_INFO: return QOL_COLOR_INFO;
        case QOL_LOG_EXEC: return QOL_COLOR_EXEC;
        case QOL_LOG_HINT: return QOL_COLOR_HINT;
        case QOL_LOG_WARN: return QOL_COLOR_WARN;
        case QOL_LOG_ERRO: return QOL_COLOR_ERRO;
        case QOL_LOG_DEAD: return QOL_COLOR_DEAD;
        default:           return QOL_COLOR_RESET;
        }
    }

    QOLDEF void qol_log(qol_log_level_t level, const char *fmt, ...) {
        qol_init_mutexes();
        QOL_MUTEX_LOCK(qol_logger_mutex);

        // Check level and read settings atomically
        FILE *log_file = qol_log_file;

        bool should_log = false;
        if (qol_logger_only_mode) {
            should_log = (level == qol_logger_only_level);
        } else {
            should_log = (level >= qol_logger_min_level && level < QOL_LOG_NONE);
        }

        if (!should_log) {
            QOL_MUTEX_UNLOCK(qol_logger_mutex);
            return;
        }

        const char *level_str = qol_level_to_str(level);

        // Only emit escape sequences when colors are enabled, so redirected
        // output (files, pipes) stays clean
        bool any_color = qol_logger_color || qol_logger_time_color;
        const char *level_color = qol_logger_color ? qol_level_to_color(level) : "";
        const char *time_color = qol_logger_time_color ? QOL_DIM : (any_color ? QOL_COLOR_RESET : "");
        const char *reset = any_color ? QOL_COLOR_RESET : "";

        char time_buf[32] = {0};
        if (qol_logger_time) {
            time_t t = time(NULL);
            struct tm *lt = localtime(&t);
            strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", lt);
            fprintf(stderr, "%s[%s]%s %s >>> %s", level_color, level_str, time_color, time_buf, reset);
        } else {
            fprintf(stderr, "%s[%s]%s ", level_color, level_str, reset);
        }

        // Write to log file (without color codes) - protect file access
        if (log_file != NULL) {
            if (qol_logger_time) {
                fprintf(log_file, "[%s] %s >>> ", level_str, time_buf);
            } else {
                fprintf(log_file, "[%s] ", level_str);
            }
        }

        // Process variadic arguments: Extract arguments from ... parameter list
        va_list args;
        va_start(args, fmt);

        // Special formatting for ERRO and DEAD levels: Display ASCII art "ship sinking" message
        // This makes critical errors highly visible and memorable
        if (level == QOL_LOG_DEAD) {
            fprintf(stderr, "\t\n");
            fprintf(stderr, "\t\n");
            fprintf(stderr, "\t              |    |    |                 \n");
            fprintf(stderr, "\t             )_)  )_)  )_)                "QOL_BOLD"Leaving the Ship!\n"QOL_RESET);
            fprintf(stderr, "\t            )___))___))___)               > ");
            vfprintf(stderr, fmt, args);
            fprintf(stderr, "\t           )____)____)_____)              \n");
            fprintf(stderr, "\t         _____|____|____|_____            \n");
            fprintf(stderr, "\t---------\\                   /---------  \n");
            fprintf(stderr, "\t  ^^^^^ ^^^^^^^^^^^^^^^^^^^^^             \n");
            fprintf(stderr, "\t    ^^^^      ^^^^     ^^^    ^^          \n");
            fprintf(stderr, "\t         ^^^^      ^^^                    \n");
            fprintf(stderr, "\t\n");

            // Write error message to log file (plain text, no ASCII art for readability)
            // va_copy is needed because va_list can only be traversed once per va_start
            if (log_file != NULL) {
                va_list args_copy;
                va_copy(args_copy, args);  // Copy va_list for second traversal
                vfprintf(log_file, fmt, args_copy);
                va_end(args_copy);         // Clean up copied va_list
                fflush(log_file);   // Ensure message is written immediately
            }
        } else {
            // Normal log levels: Just print the message with formatting
            vfprintf(stderr, fmt, args);

            // Write message to log file (plain text, no color codes)
            // va_copy allows us to traverse va_list twice (once for stderr, once for file)
            if (log_file != NULL) {
                va_list args_copy;
                va_copy(args_copy, args);  // Copy va_list for second traversal
                vfprintf(log_file, fmt, args_copy);
                va_end(args_copy);         // Clean up copied va_list
                fflush(log_file);  // Ensure message is written immediately
            }
        }

        va_end(args);  // Clean up original va_list
        QOL_MUTEX_UNLOCK(qol_logger_mutex);

        // Handle fatal log level
        if (level == QOL_LOG_DEAD) {
            fflush(NULL);           // Flush all output streams before abort
            exit(EXIT_FAILURE);     // Clean exit with failure status
            // abort();                // Immediate termination (may generate core dump)
        }
    }

    //////////////////////////////////////////////////
    /// CLI_PARSER ///////////////////////////////////
    //////////////////////////////////////////////////

    QOLDEF void qol_init_argparser(int argc, char *argv[]) {
        qol_init_mutexes();
        // Register built-in --help argument (no default value, flag-style)
        qol_add_argument("--help", NULL, "Show this help message");

        // Parse each command-line argument (skip argv[0] which is program name)
        QOL_MUTEX_LOCK(qol_argparser_mutex);
        int parser_count = qol_parser.count;
        QOL_MUTEX_UNLOCK(qol_argparser_mutex);
        for (int i = 1; i < argc; i++) {
            // Check against all registered arguments
            QOL_MUTEX_LOCK(qol_argparser_mutex);
            // Re-check count in case it changed
            parser_count = qol_parser.count;
            for (int j = 0; j < parser_count; j++) {
                qol_arg_t *arg = &qol_parser.args[j];

                // Long option match: Check if argv[i] matches --long_name format
                if (strcmp(argv[i], arg->long_name) == 0) {
                    if (strcmp(arg->long_name, "--help") == 0) {
                        arg->value = "1"; // Help is a flag, set to "1" to indicate it's set
                    } else if (i + 1 < argc && argv[i + 1][0] != '-') {
                        // Next argument exists and doesn't start with '-' (it's a value, not an option)
                        arg->value = argv[i + 1];
                        i++; // Skip the value argument in next iteration
                    } else {
                        // No value provided, treat as flag (set to "1")
                        arg->value = "1";
                    }
                }
                // Short option match: Check if argv[i] is "-X" where X matches short_name
                else if (argv[i][0] == '-' && argv[i][1] == arg->short_name) {
                    if (arg->short_name == 'h') {
                        arg->value = "1"; // Help flag
                    } else if (i + 1 < argc && argv[i + 1][0] != '-') {
                        // Next argument is a value
                        arg->value = argv[i + 1];
                        i++; // Skip the value argument
                    } else {
                        // No value provided, treat as flag
                        arg->value = "1";
                    }
                }
            }
            QOL_MUTEX_UNLOCK(qol_argparser_mutex);
        }

        // Show help message if --help was specified, then exit
        qol_arg_t *help = qol_get_argument("--help");
        if (help && help->value) {
            printf("Usage:\n");
            QOL_MUTEX_LOCK(qol_argparser_mutex);
            // Print all registered arguments with their help text
            int parser_count = qol_parser.count;
            for (int i = 0; i < parser_count; i++) {
                qol_arg_t *arg = &qol_parser.args[i];
                printf("  %s, -%c: %s (default: %s)\n",
                    arg->long_name,
                    arg->short_name,
                    arg->help_msg ? arg->help_msg : "",
                    arg->default_val ? arg->default_val : "none");
            }
            QOL_MUTEX_UNLOCK(qol_argparser_mutex);
            exit(0); // Exit successfully after showing help
        }
    }

    // Global argument parser instance: Stores all registered command-line arguments
    // Initialized to zero (empty). Persists throughout program execution.
    qol_argparser_t qol_parser = { .count = 0 };

    QOLDEF void qol_add_argument(const char *long_name, const char *default_val, const char *help_msg) {
        qol_init_mutexes();
        QOL_MUTEX_LOCK(qol_argparser_mutex);
        // Check if we've reached the maximum number of arguments
        if (qol_parser.count >= QOL_ARG_MAX) {
            QOL_MUTEX_UNLOCK(qol_argparser_mutex);
            qol_log(QOL_LOG_ERRO, "Maximum number of arguments reached\n");
            return;
        }
        // Get pointer to next available argument slot and increment count
        qol_arg_t *arg = &qol_parser.args[qol_parser.count++];
        arg->long_name = long_name;
        arg->short_name = long_name[2]; // Extract short name: "--output" -> 'o' (3rd character)
        arg->default_val = default_val;
        arg->help_msg = help_msg;
        arg->value = default_val; // Initialize value to default (will be overwritten if found in argv)
        QOL_MUTEX_UNLOCK(qol_argparser_mutex);
    }

    QOLDEF qol_arg_t *qol_get_argument(const char *long_name) {
        qol_init_mutexes();
        QOL_MUTEX_LOCK(qol_argparser_mutex);
        // Linear search through registered arguments
        qol_arg_t *result = NULL;
        for (int i = 0; i < qol_parser.count; i++) {
            if (strcmp(qol_parser.args[i].long_name, long_name) == 0) {
                result = &qol_parser.args[i];
                break;
            }
        }
        QOL_MUTEX_UNLOCK(qol_argparser_mutex);
        return result; // Argument not found
    }

    QOLDEF int qol_arg_as_int(qol_arg_t *arg) {
        if (!arg || !arg->value) return EXIT_SUCCESS;
        return atoi(arg->value);
    }

    QOLDEF const char *qol_arg_as_string(qol_arg_t *arg) {
        if (!arg) return "";
        return arg->value ? arg->value : "";
    }

    //////////////////////////////////////////////////
    /// NO_BUILD /////////////////////////////////////
    //////////////////////////////////////////////////

    QOLDEF void qol_ensure_dir_for_file(const char* filepath) {
        char dir[1024];
        // Copy filepath to mutable buffer (with size limit protection)
        strncpy(dir, filepath, sizeof(dir));
        dir[sizeof(dir)-1] = '\0'; // Ensure null termination

        // Find the last path separator (Unix style)
        char *slash = strrchr(dir, '/');
#if defined(WINDOWS)
        // On Windows, also check for backslash separator
        if (!slash) slash = strrchr(dir, '\\');
#endif
        if (slash) {
            *slash = '\0'; // Null-terminate at separator (extract directory portion)
            qol_mkdir_if_not_exists(dir); // Create directory if it doesn't exist
        }
        // If no separator found, file is in current directory (no action needed)
    }

    QOLDEF char* qol_default_compiler_flags(void) {
#if defined(WINDOWS)
        return ""; // Windows doesn't use these flags (different compiler)
#elif defined(__APPLE__) && defined(__MACH__)
        return "-Wall -Wextra"; // macOS: Enable all warnings and extra warnings
#elif defined(__linux__)
        return "-Wall -Wextra"; // Linux: Enable all warnings and extra warnings
#else
        return ""; // Unknown platform: no flags
#endif
    }

    QOLDEF QOL_Cmd qol_default_c_build_extended(const char *source, const char *output, const char *flags[], size_t flags_count, const char *compiler) {
        QOL_Cmd cmd = {0};
        qol_push(&cmd, compiler ? compiler : "cc");
        // Flags go before the source/output pair, matching conventional compiler invocations
        for (size_t i = 0; i < flags_count; i++) {
            qol_push(&cmd, flags[i]);
        }
        qol_push(&cmd, source);
        qol_push(&cmd, "-o");
        if (output) {
            qol_push(&cmd, output);
        } else {
            // Auto-generate output name from the source filename (like qol_default_c_build).
            // NOTE: the string is intentionally NOT freed here - the command keeps a
            // pointer to it until the command is executed and released.
            char *auto_output = qol_get_filename_no_ext(source);
            if (auto_output) qol_push(&cmd, auto_output);
        }
        return cmd;
    }

    QOLDEF QOL_Cmd qol_default_c_build(const char *source, const char *output) {
        QOL_Cmd cmd = {0}; // Initialize command structure to zero

        // Select compiler based on platform
#if defined(WINDOWS)
        qol_push(&cmd, "gcc"); // Windows: Use GCC (MinGW/MSYS2)
#elif defined(__APPLE__) && defined(__MACH__)
        qol_push(&cmd, "cc"); // macOS: Use system default C compiler (usually Clang)
#elif defined(__linux__)
        qol_push(&cmd, "cc"); // Linux: Use system default C compiler (usually GCC)
#else
        qol_push(&cmd, "cc"); // Fallback: Use cc (should work on most Unix systems)
#endif

        // Push compiler flags as separate arguments (each flag is a separate argv element)
        // Only add flags on Unix-like systems (Windows compilers use different syntax)
#if !defined(_WIN32) && !defined(_WIN64)
        qol_push(&cmd, "-Wall");  // Enable all common warnings
        qol_push(&cmd, "-Wextra"); // Enable extra warnings
#endif

        // Add source file and output flag
        qol_push(&cmd, source);  // Source file path
        qol_push(&cmd, "-o");    // Output flag

        // Add output filename (or auto-generate from source)
        if (output) {
            qol_push(&cmd, output); // Use provided output name
        } else {
            // Auto-generate output name: remove extension from source filename.
            // NOTE: the string is intentionally NOT freed here - the command stores the
            // pointer and would otherwise dangle (use-after-free) when executed later.
            // The small allocation lives until the process exits, which is acceptable
            // for a build script.
            char *auto_output = qol_get_filename_no_ext(source);
            if (auto_output) qol_push(&cmd, auto_output);
        }

        return cmd; // Return constructed command structure
    }

    QOLDEF bool qol_is_path1_modified_after_path2(const char *path1, const char *path2) {
        struct stat stat1, stat2;

        // Get file stats (modification time)
        if (stat(path1, &stat1) != 0) return false; // path1 doesn't exist or error
        if (stat(path2, &stat2) != 0) return true;  // path2 doesn't exist, path1 is "newer"

        // Compare modification times: difftime returns positive if stat1 is newer
        return difftime(stat1.st_mtime, stat2.st_mtime) > 0;
    }

    QOLDEF char *qol_get_filename_no_ext(const char *path) {
        // Find last path separator (Unix style)
        const char *slash = strrchr(path, '/');
        // Find last path separator (Windows style)
        const char *backslash = strrchr(path, '\\');
        const char *base = path; // Default: entire path is filename

        // Determine basename: use the rightmost separator (handles mixed paths)
        if (slash || backslash) {
            if (slash && backslash)
                base = (slash > backslash) ? slash + 1 : backslash + 1; // Use rightmost separator
            else if (slash)
                base = slash + 1; // Unix separator
            else
                base = backslash + 1; // Windows separator
        }

        // Copy basename to new buffer
        char *copy = strdup(base);
        if (!copy) return NULL; // Allocation failed

        // Remove extension: find last dot and null-terminate there
        char *dot = strrchr(copy, '.');
        if (dot) *dot = '\0'; // Remove extension (e.g., "file.c" -> "file")

        return copy; // Caller must free this string
    }

    QOLDEF void qol_auto_rebuild(const char *src) {
        if (!src) return;

        struct stat src_attr, out_attr;

#if defined(WINDOWS)
        char *out = "build_new.exe";
#else
        char *out = qol_get_filename_no_ext(src);
        if (!out) return;
#endif

        if (stat(src, &src_attr) != 0) {
            qol_log(QOL_LOG_ERRO, "No such file or directory (%s).\n", src);
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
            qol_log(QOL_LOG_DIAG, "Rebuilding: %s -> %s\n", src, out);
#if defined(MACOS) || defined(LINUX)
            QOL_Cmd own_build = qol_default_c_build(src, out);
            if (!qol_run_always(&own_build)) {
                qol_release(&own_build);
                qol_log(QOL_LOG_ERRO, "Rebuild failed.\n");
#if !defined(_WIN32) && !defined(_WIN64)
                free(out);
#endif
                exit(1);
            }
            qol_release(&own_build);

            qol_log(QOL_LOG_DIAG, "Restarting with updated build executable...\n");
            char *restart_argv[] = {out, NULL};
            execv(out, restart_argv);
            qol_log(QOL_LOG_ERRO, "Failed to restart build process.\n");
#if !defined(_WIN32) && !defined(_WIN64)
            free(out);
#endif
            exit(1);
#elif defined(WINDOWS)
            QOL_Cmd own_build = qol_default_c_build(src, out);
            if (!qol_run_always(&own_build)) {
                qol_release(&own_build);
                qol_log(QOL_LOG_ERRO, "Rebuild failed.\n");
                exit(1);
            }
            qol_release(&own_build);

            qol_log(QOL_LOG_DIAG, "Restarting with updated build executable...\n");
            STARTUPINFO si = { sizeof(si) };
            PROCESS_INFORMATION pi;
            if (!CreateProcess(out, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
                qol_log(QOL_LOG_ERRO, "Failed to restart build process.\n");
                exit(1);
            }
            ExitProcess(0);
#else
            #error Unsupported platform
#endif
        } else {
            qol_log(QOL_LOG_DIAG, "Up to date: %s\n", out);
#if !defined(_WIN32) && !defined(_WIN64)
            free(out);
#endif
        }
    }

    QOLDEF void qol_auto_rebuild_plus_impl(const char *src, ...) {
        if (!src) return;
        struct stat src_attr, out_attr;
#if defined(WINDOWS)
        const char *out = "build_new.exe";
#else
        char *out = qol_get_filename_no_ext(src);
#endif
        if (stat(src, &src_attr) != 0) {
            qol_log(QOL_LOG_ERRO, "No such file or directory (%s).\n", src);
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

        // Check additional dependencies from variadic arguments (only if source check didn't trigger rebuild)
        // This allows us to skip dependency checking if source already requires rebuild
        if (!need_rebuild) {
            va_list args;
            va_start(args, src); // Start variadic argument processing (src is first fixed arg)
            const char *dep_file = va_arg(args, const char*); // Get first dependency
            // Iterate through all dependencies until NULL terminator
            while (dep_file != NULL) {
                // Check if this dependency is newer than output
                if (qol_is_path1_modified_after_path2(dep_file, out)) {
                    qol_log(QOL_LOG_DIAG, "Dependency %s is newer than binary, rebuild needed\n", dep_file);
                    need_rebuild = true;
                    // Don't break - continue checking all dependencies for complete logging
                    // This helps users understand which dependencies triggered the rebuild
                }
                dep_file = va_arg(args, const char*); // Get next dependency
            }
            va_end(args); // Clean up variadic argument list
        }

        if (need_rebuild) {
            qol_log(QOL_LOG_DIAG, "Rebuilding: %s -> %s\n", src, out);

#if defined(MACOS) || defined(LINUX)
            QOL_Cmd own_build = qol_default_c_build(src, out);
            if (!qol_run_always(&own_build)) {
                qol_release(&own_build);
                qol_log(QOL_LOG_ERRO, "Rebuild failed.\n");
#if !defined(_WIN32) && !defined(_WIN64)
                free(out);
#endif
                exit(1);
            }
            qol_release(&own_build);

            qol_log(QOL_LOG_DIAG, "Restarting with updated build executable...\n");
            char *restart_argv[] = {out, NULL};
            execv(out, restart_argv);
            qol_log(QOL_LOG_ERRO, "Failed to restart build process.\n");
#if !defined(_WIN32) && !defined(_WIN64)
            free(out);
#endif
            exit(1);
#elif defined(WINDOWS)
            QOL_Cmd own_build = qol_default_c_build(src, out);
            if (!qol_run_always(&own_build)) {
                qol_release(&own_build);
                qol_log(QOL_LOG_ERRO, "Rebuild failed.\n");
                exit(1);
            }
            qol_release(&own_build);

            qol_log(QOL_LOG_DIAG, "Restarting with updated build executable...\n");
            STARTUPINFO si = { sizeof(si) };
            PROCESS_INFORMATION pi;
            char cmdline[1024];
            snprintf(cmdline, sizeof(cmdline), "\"%s\"", out);
            if (!CreateProcess(NULL, cmdline, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
                qol_log(QOL_LOG_ERRO, "Failed to restart build process.\n");
                exit(1);
            }
            ExitProcess(0);
#else
            #error Unsupported platform
#endif
        } else {
            qol_log(QOL_LOG_DIAG, "Up to date: %s\n", out);
#if !defined(_WIN32) && !defined(_WIN64)
            free(out);
#endif
        }
    }

    QOLDEF const char* qol_cmd_get_source(QOL_Cmd* cmd) {
        if (!cmd || !cmd->data || cmd->len < 2) return NULL;

        for (size_t i = 0; i < cmd->len - 1; i++) {
            if (cmd->data[i] && strcmp(cmd->data[i], "-o") == 0) {
                if (i > 0) {
                    for (size_t j = 1; j < i; j++) {
                        if (cmd->data[j] && strstr(cmd->data[j], ".c") != NULL) {
                            return cmd->data[j];
                        }
                    }
                    return cmd->data[i - 1];
                }
            }
        }

        for (size_t i = 1; i < cmd->len; i++) {
            if (cmd->data[i] && strstr(cmd->data[i], ".c") != NULL) {
                return cmd->data[i];
            }
        }

        return NULL;
    }

    QOLDEF const char* qol_cmd_get_output(QOL_Cmd* cmd) {
        if (!cmd || !cmd->data || cmd->len < 2) return NULL;

        for (size_t i = 0; i < cmd->len - 1; i++) {
            if (cmd->data[i] && strcmp(cmd->data[i], "-o") == 0) {
                return cmd->data[i + 1];
            }
        }

        return NULL;
    }

#ifdef WINDOWS
    QOLDEF char *qol_win32_error_message(DWORD err) {
        qol_init_mutexes();
        QOL_MUTEX_LOCK(qol_win32_err_mutex);
        static char win32ErrMsg[QOL_WIN32_ERR_BUFFER_SIZE] = {0};
        DWORD errMsgSize = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, err, LANG_USER_DEFAULT, win32ErrMsg,
                                          sizeof(win32ErrMsg), NULL);

        if (errMsgSize == 0) {
            if (GetLastError() != ERROR_MR_MID_NOT_FOUND) {
                if (snprintf(win32ErrMsg, sizeof(win32ErrMsg), "Could not get error message for 0x%lX", err) > 0) {
                    QOL_MUTEX_UNLOCK(qol_win32_err_mutex);
                    return (char *)&win32ErrMsg;
                } else {
                    QOL_MUTEX_UNLOCK(qol_win32_err_mutex);
                    return NULL;
                }
            } else {
                if (snprintf(win32ErrMsg, sizeof(win32ErrMsg), "Invalid Windows Error code (0x%lX)", err) > 0) {
                    QOL_MUTEX_UNLOCK(qol_win32_err_mutex);
                    return (char *)&win32ErrMsg;
                } else {
                    QOL_MUTEX_UNLOCK(qol_win32_err_mutex);
                    return NULL;
                }
            }
        }

        while (errMsgSize > 1 && isspace(win32ErrMsg[errMsgSize - 1])) {
            win32ErrMsg[--errMsgSize] = '\0';
        }

        QOL_MUTEX_UNLOCK(qol_win32_err_mutex);
        return win32ErrMsg;
    }
#endif

    QOLDEF void qol_cmd_log(QOL_Cmd* cmd) {
        if (!cmd || !cmd->data || cmd->len == 0) return;

        // TODO: instead of printing the full command every time, only print a short variant? Basically this
        //       `cc -O3 -Wall -Wextra -I./libs/raylib-5.5_macos/include -c main.c -o out/main.o`
        //       would be transformed into something like `cc ... -c main.c -o out/main.o` or more UNIX
        //       Makefile like into `CC main.c`.

        char command[QOL_EXEC_BUFFER_SIZE] = {0};
        size_t pos = 0;
        bool truncated = false;
        for (size_t i = 0; i < cmd->len; i++) {
            if (!cmd->data[i]) continue;
            if (pos > 0 && pos < sizeof(command) - 1) {
                command[pos++] = ' ';
            }
            const char *item = cmd->data[i];
            size_t item_len = strlen(item);
            if (pos + item_len < sizeof(command) - 1) {
                strncpy(command + pos, item, sizeof(command) - pos - 1);
                pos += item_len;
            } else {
                truncated = true;
                break; // Buffer full, stop adding arguments
            }
        }
        command[sizeof(command) - 1] = '\0'; // Ensure null termination
        if (truncated) {
            qol_log(QOL_LOG_WARN, "Command truncated (exceeds %d bytes): %s...\n", QOL_EXEC_BUFFER_SIZE - 1, command);
        }
        qol_log(QOL_LOG_EXEC, "%s\n", command);
    }

    QOLDEF QOL_Proc qol_cmd_execute_async(QOL_Cmd* cmd) {
        if (!cmd || !cmd->data || cmd->len == 0) {
            qol_log(QOL_LOG_ERRO, "Invalid command: empty or null\n");
            return QOL_INVALID_PROC;
        }

        qol_cmd_log(cmd);

#ifdef WINDOWS
        // Windows: CreateProcess requires a single command-line string, not an array
        // Arguments with spaces must be quoted. Example: "cc -Wall main.c -o main"
        char cmdline[QOL_EXEC_BUFFER_SIZE] = {0};
        size_t pos = 0;
        bool truncated = false;
        for (size_t i = 0; i < cmd->len; ++i) {
            // Add space separator before each argument (except first)
            if (i > 0 && pos < sizeof(cmdline) - 1) cmdline[pos++] = ' ';
            const char *arg = cmd->data[i];
            // Quote arguments that contain spaces or tabs (required by Windows)
            if (strchr(arg, ' ') || strchr(arg, '\t')) {
                if (pos >= sizeof(cmdline) - 1) { truncated = true; break; }
                cmdline[pos++] = '"'; // Opening quote
                size_t len = strlen(arg);
                // Copy argument to buffer (with bounds checking)
                if (pos + len < sizeof(cmdline) - 1) {
                    strncpy(cmdline + pos, arg, sizeof(cmdline) - pos - 1);
                    pos += len;
                } else {
                    truncated = true;
                    break;
                }
                if (pos >= sizeof(cmdline) - 1) { truncated = true; break; }
                cmdline[pos++] = '"'; // Closing quote
            } else {
                // No spaces, copy directly without quotes
                size_t len = strlen(arg);
                if (pos + len < sizeof(cmdline) - 1) {
                    strncpy(cmdline + pos, arg, sizeof(cmdline) - pos - 1);
                    pos += len;
                } else {
                    truncated = true;
                    break;
                }
            }
        }
        cmdline[sizeof(cmdline) - 1] = '\0'; // Ensure null termination
        if (truncated) {
            qol_log(QOL_LOG_ERRO, "Command line truncated (exceeds %d bytes), command execution may fail\n", QOL_EXEC_BUFFER_SIZE - 1);
            return QOL_INVALID_PROC;
        }

        // Create process using Windows API
        STARTUPINFO si = { sizeof(si) }; // Startup info (zero-initialized)
        PROCESS_INFORMATION pi; // Process info (filled by CreateProcess)
        ZeroMemory(&pi, sizeof(pi)); // Zero-initialize process info

        // CreateProcess: NULL for application name (use cmdline), cmdline contains full command
        // Returns process handle and thread handle in PROCESS_INFORMATION
        BOOL success = CreateProcessA(NULL, cmdline, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
        if (!success) {
            qol_log(QOL_LOG_ERRO, "Could not create process: %s\n", qol_win32_error_message(GetLastError()));
            return QOL_INVALID_PROC;
        }

        // Close thread handle (we only need process handle for waiting)
        CloseHandle(pi.hThread);
        return pi.hProcess; // Return process handle for later waiting
#else
        // Unix: Fork and exec approach
        pid_t pid = fork(); // Create child process
        if (pid < 0) {
            // Fork failed
            qol_log(QOL_LOG_ERRO, "Could not fork process: %s\n", strerror(errno));
            return QOL_INVALID_PROC;
        }

        if (pid == 0) {
            // Child process: Replace process image with command
            // Build NULL-terminated argument array for execvp
            QOL_Cmd cmd_null = {0};
            for (size_t i = 0; i < cmd->len; i++) {
                qol_push(&cmd_null, cmd->data[i]); // Copy all arguments
            }
            qol_push(&cmd_null, NULL); // NULL terminator required by execvp

            // execvp: Replace current process with command (searches PATH for executable)
            // Never returns on success (process image replaced)
            if (execvp(cmd->data[0], (char * const*) cmd_null.data) < 0) {
                // execvp failed (shouldn't happen if command exists)
                // Free cmd_null before exiting to avoid memory leak
                qol_release(&cmd_null);
                qol_log(QOL_LOG_ERRO, "Could not exec process: %s\n", strerror(errno));
                exit(1); // Exit child process with error
            }
            QOL_UNREACHABLE("qol_cmd_execute_async"); // execvp never returns on success
        }

        // Parent process: Return child PID for later waiting
        return pid;
#endif
    }

    QOLDEF bool qol_proc_wait(QOL_Proc proc) {
        if (proc == QOL_INVALID_PROC) return false;

#ifdef WINDOWS
        DWORD result = WaitForSingleObject(proc, INFINITE);
        if (result == WAIT_FAILED) {
            qol_log(QOL_LOG_ERRO, "Could not wait on child process: %s\n", qol_win32_error_message(GetLastError()));
            CloseHandle(proc);
            return false;
        }

        DWORD exit_code;
        if (!GetExitCodeProcess(proc, &exit_code)) {
            qol_log(QOL_LOG_ERRO, "Could not get process exit code: %s\n", qol_win32_error_message(GetLastError()));
            CloseHandle(proc);
            return false;
        }

        CloseHandle(proc);

        if (exit_code != 0) {
            qol_log(QOL_LOG_ERRO, "Command failed with exit code %lu\n", exit_code);
            return false;
        }

        return true;
#else
        int wstatus;
        if (waitpid(proc, &wstatus, 0) < 0) {
            qol_log(QOL_LOG_ERRO, "Could not wait for process: %s\n", strerror(errno));
            return false;
        }

        if (WIFEXITED(wstatus)) {
            int exit_code = WEXITSTATUS(wstatus);
            if (exit_code != 0) {
                qol_log(QOL_LOG_ERRO, "Command failed with exit code %d\n", exit_code);
                return false;
            }
        } else if (WIFSIGNALED(wstatus)) {
            qol_log(QOL_LOG_ERRO, "Command terminated by signal %d\n", WTERMSIG(wstatus));
            return false;
        }

        return true;
#endif
    }

    QOLDEF bool qol_procs_wait(QOL_Procs *procs) {
        if (!procs) return false;

        bool all_success = true;
        for (size_t i = 0; i < procs->len; i++) {
            if (procs->data[i] != QOL_INVALID_PROC) {
                if (!qol_proc_wait(procs->data[i])) {
                    all_success = false;
                }
            }
        }
        procs->len = 0;
        return all_success;
    }

    QOLDEF bool qol_run_impl(QOL_Cmd* config, QOL_RunOptions opts) {
        if (!config || !config->data || config->len == 0) {
            qol_log(QOL_LOG_ERRO, "Invalid build configuration\n");
            if (config) qol_release(config);
            return false;
        }

        const char *source = qol_cmd_get_source(config);
        const char *output = qol_cmd_get_output(config);

        if (!source || !output) {
            qol_log(QOL_LOG_DIAG, "Could not extract source or output from command. Run the command anyway.\n");
            // TODO: should we rather exit with false and do something like this?
            // if (opts.cmd) return qol_run_always_impl(config, opts);
            return qol_run_always_impl(config, opts);
        }

        qol_ensure_dir_for_file(output);

        if (!qol_is_path1_modified_after_path2(source, output)) {
            qol_log(QOL_LOG_DIAG, "Up to date: %s\n", output);
            qol_release(config);
            return true;
        }

        return qol_run_always_impl(config, opts);
    }

    QOLDEF bool qol_run_always_impl(QOL_Cmd* config, QOL_RunOptions opts) {
        if (!config || !config->data || config->len == 0) {
            qol_log(QOL_LOG_ERRO, "Invalid build configuration\n");
            if (config) qol_release(config);
            return false;
        }

        QOL_Proc proc = qol_cmd_execute_async(config);
        if (proc == QOL_INVALID_PROC) {
            qol_release(config);
            return false;
        }

        if (opts.procs) {
            // Async mode: hand the process handle to the caller for later qol_procs_wait()
            qol_push(opts.procs, proc);
            qol_release(config);
            return true;
        }

        // Sync mode: wait for the process to finish before returning
        bool success = qol_proc_wait(proc);
        qol_release(config);
        return success;
    }

    //////////////////////////////////////////////////
    /// TEMP_ALLOCATOR ///////////////////////////////
    //////////////////////////////////////////////////

    // Temporary allocator state: Stack-like memory allocator using fixed-size buffer
    // All allocations are automatically freed when qol_temp_reset() is called
    // No manual memory management needed - perfect for temporary strings and buffers
    static size_t qol_temp_size = 0; // Current allocation offset (bytes allocated so far)
    char qol_temp[QOL_TEMP_CAPACITY] = {0}; // Fixed-size buffer (8MB default)

    QOLDEF char *qol_temp_strdup(const char *cstr) {
        if (!cstr) return NULL;
        size_t n = strlen(cstr);
        char *result = qol_temp_alloc(n + 1);
        if (!result) return NULL; // Return NULL instead of aborting when temp allocator is full
        memcpy(result, cstr, n);
        result[n] = '\0';
        return result;
    }

    QOLDEF void *qol_temp_alloc(size_t size) {
        qol_init_mutexes();
        // Round up to 16 bytes so returned pointers are suitably aligned for any type
        if (size > (size_t)-1 - 15) return NULL; // Overflow guard
        size = (size + 15) & ~(size_t)15;
        QOL_MUTEX_LOCK(qol_temp_alloc_mutex);
        if (size > QOL_TEMP_CAPACITY - qol_temp_size) {
            QOL_MUTEX_UNLOCK(qol_temp_alloc_mutex);
            return NULL;
        }
        void *result = &qol_temp[qol_temp_size];
        qol_temp_size += size;
        QOL_MUTEX_UNLOCK(qol_temp_alloc_mutex);
        return result;
    }

    QOLDEF char *qol_temp_sprintf(const char *format, ...) {
        if (!format) return NULL;
        va_list args;
        // First pass: Determine required buffer size
        // vsnprintf with NULL buffer returns number of characters needed (excluding null terminator)
        va_start(args, format);
        int n = vsnprintf(NULL, 0, format, args);
        va_end(args);

        if (n < 0) return NULL; // Formatting error, return NULL instead of aborting
        // Allocate buffer: n characters + 1 for null terminator
        char *result = qol_temp_alloc(n + 1);
        if (!result) return NULL; // Return NULL instead of aborting when temp allocator is full
        // Second pass: Format string into allocated buffer
        // Must restart va_list because it can only be traversed once per va_start
        va_start(args, format);
        vsnprintf(result, n + 1, format, args); // Write formatted string to buffer
        va_end(args);

        return result; // Return temp-allocated formatted string
    }

    QOLDEF void qol_temp_reset(void) {
        qol_init_mutexes();
        QOL_MUTEX_LOCK(qol_temp_alloc_mutex);
        qol_temp_size = 0;
        QOL_MUTEX_UNLOCK(qol_temp_alloc_mutex);
    }

    size_t qol_temp_save(void) {
        qol_init_mutexes();
        QOL_MUTEX_LOCK(qol_temp_alloc_mutex);
        size_t checkpoint = qol_temp_size;
        QOL_MUTEX_UNLOCK(qol_temp_alloc_mutex);
        return checkpoint;
    }

    QOLDEF void qol_temp_rewind(size_t checkpoint) {
        qol_init_mutexes();
        QOL_MUTEX_LOCK(qol_temp_alloc_mutex);
        qol_temp_size = checkpoint;
        QOL_MUTEX_UNLOCK(qol_temp_alloc_mutex);
    }

    //////////////////////////////////////////////////
    /// AUTO_FREE ////////////////////////////////////
    //////////////////////////////////////////////////

    // Generic cleanup function that works with any pointer type.
    // Uses a union to allow type-punning for the cleanup attribute's strict type checking.
    // The cleanup attribute passes the address of the variable, which we cast to void**.
    static inline void _qol_auto_free_impl(void *p) {
        // Cast the void* to void** to access the pointer value
        void **ptr = (void **)p;
        if (ptr && *ptr) {
            qol_diag("Auto-free: freeing memory at %p\n", *ptr);
            free(*ptr);
            *ptr = NULL;
        }
    }

    //////////////////////////////////////////////////
    /// FILE_OPS /////////////////////////////////////
    //////////////////////////////////////////////////

    QOLDEF bool qol_mkdir_if_not_exists(const char *path) {
        struct stat st;
#if defined(WINDOWS)
        if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES) {
            return true;
        }
#else
        if (stat(path, &st) == 0) {
            return true;
        }
#endif
        return qol_mkdir(path);
    }

    QOLDEF bool qol_mkdir(const char *path) {
#ifdef _WIN32
        int result = _mkdir(path);
#else
        int result = mkdir(path, 0755);
#endif
        if (result != 0) {
            qol_log(QOL_LOG_ERRO, "Failed to create directory: %s\n", path);
            return false;
        }
        qol_log(QOL_LOG_INFO, "Created directory `%s/`\n", path);
        return true;
    }

    QOLDEF bool qol_copy_file(const char *src_path, const char *dst_path) {
        if (!src_path || !dst_path) return false;

        FILE *src = fopen(src_path, "rb");
        if (!src) {
            qol_log(QOL_LOG_ERRO, "Failed to open source file: %s\n", src_path);
            return false;
        }

        FILE *dst = fopen(dst_path, "wb");
        if (!dst) {
            qol_log(QOL_LOG_ERRO, "Failed to open destination file: %s\n", dst_path);
            fclose(src);
            return false;
        }

        char buffer[4096];
        size_t bytes_read;
        while ((bytes_read = fread(buffer, 1, sizeof(buffer), src)) > 0) {
            if (fwrite(buffer, 1, bytes_read, dst) != bytes_read) {
                qol_log(QOL_LOG_ERRO, "Failed to write to destination file\n");
                fclose(src);
                fclose(dst);
                return false;
            }
        }

        fclose(src);
        fclose(dst);
        qol_log(QOL_LOG_INFO, "Copied %s to %s\n", src_path, dst_path);
        return true;
    }

    QOLDEF bool qol_copy_dir_rec(const char *src_path, const char *dst_path) {
        if (!src_path || !dst_path) return false;

#if defined(MACOS) || defined(LINUX)
        DIR *dir = opendir(src_path);
        if (!dir) {
            qol_log(QOL_LOG_ERRO, "Failed to open source directory: %s\n", src_path);
            return false;
        }

        if (!qol_mkdir_if_not_exists(dst_path)) {
            closedir(dir);
            return false;
        }

        struct dirent *entry;
        char src_file[QOL_PATH_BUFFER_SIZE];
        char dst_file[QOL_PATH_BUFFER_SIZE];

        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;

            if (snprintf(src_file, sizeof(src_file), "%s/%s", src_path, entry->d_name) >= (int)sizeof(src_file)) {
                qol_log(QOL_LOG_ERRO, "Source path too long: %s/%s\n", src_path, entry->d_name);
                closedir(dir);
                return false;
            }
            if (snprintf(dst_file, sizeof(dst_file), "%s/%s", dst_path, entry->d_name) >= (int)sizeof(dst_file)) {
                qol_log(QOL_LOG_ERRO, "Destination path too long: %s/%s\n", dst_path, entry->d_name);
                closedir(dir);
                return false;
            }

            struct stat st;
            if (stat(src_file, &st) == 0) {
                if (S_ISDIR(st.st_mode)) {
                    if (!qol_copy_dir_rec(src_file, dst_file)) {
                        closedir(dir);
                        return false;
                    }
                } else if (S_ISREG(st.st_mode)) {
                    if (!qol_copy_file(src_file, dst_file)) {
                        closedir(dir);
                        return false;
                    }
                }
            }
        }

        closedir(dir);
        return true;
#elif defined(WINDOWS)
        WIN32_FIND_DATA find_data;
        char search_path[QOL_PATH_BUFFER_SIZE];
        if (snprintf(search_path, sizeof(search_path), "%s\\*", src_path) >= (int)sizeof(search_path)) {
            qol_log(QOL_LOG_ERRO, "Search path too long: %s\n", src_path);
            return false;
        }

        HANDLE handle = FindFirstFile(search_path, &find_data);
        if (handle == INVALID_HANDLE_VALUE) {
            qol_log(QOL_LOG_ERRO, "Failed to open source directory: %s\n", src_path);
            return false;
        }

        if (!qol_mkdir_if_not_exists(dst_path)) {
            FindClose(handle);
            return false;
        }

        do {
            if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0)
                continue;

            char src_file[QOL_PATH_BUFFER_SIZE];
            char dst_file[QOL_PATH_BUFFER_SIZE];
            if (snprintf(src_file, sizeof(src_file), "%s\\%s", src_path, find_data.cFileName) >= (int)sizeof(src_file)) {
                qol_log(QOL_LOG_ERRO, "Source path too long: %s\\%s\n", src_path, find_data.cFileName);
                FindClose(handle);
                return false;
            }
            if (snprintf(dst_file, sizeof(dst_file), "%s\\%s", dst_path, find_data.cFileName) >= (int)sizeof(dst_file)) {
                qol_log(QOL_LOG_ERRO, "Destination path too long: %s\\%s\n", dst_path, find_data.cFileName);
                FindClose(handle);
                return false;
            }

            if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                if (!qol_copy_dir_rec(src_file, dst_file)) {
                    FindClose(handle);
                    return false;
                }
            } else {
                if (!qol_copy_file(src_file, dst_file)) {
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

    QOLDEF bool qol_read_file(const char *path, QOL_String *content) {
        if (!path || !content) return false;

        FILE *fp = fopen(path, "r");
        if (!fp) return false;

        // getline automatically allocates/reallocates buffer as needed
        // line: Pointer to buffer (NULL initially, getline allocates)
        // n: Pointer to buffer size (getline updates this)
        char *line = NULL;
        size_t n = 0;

        // Read file line by line: getline returns -1 on EOF or error
        while (getline(&line, &n, fp) != -1) {
            size_t len = strlen(line);
            // Remove trailing newline if present (getline includes it)
            if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';

            // Duplicate line: getline's buffer is reused, we need our own copy
            char *copy = strdup(line);
            if (!copy) {
                // Allocation failed: clean up previously read lines and abort
                qol_release_string(content); // Free all previously allocated lines
                fclose(fp);
                free(line); // Free getline's buffer
                return false;
            }

            // Add line to dynamic array (content takes ownership of copy)
            qol_push(content, copy);
        }

        // Clean up: free getline's buffer and close file
        free(line); // getline may have allocated this buffer
        fclose(fp);
        return true;
    }

    QOLDEF bool qol_read_dir_recursive(const char *parent, QOL_String *content) {
        if (!parent || !content) return false;

        // Initialize content only on first call (when data is NULL)
        if (content->data == NULL) {
            content->data = NULL;
            content->len = 0;
            content->cap = 0;
        }

#if defined(MACOS) || defined(LINUX)
        DIR *dir = opendir(parent);
        if (!dir) {
            qol_log(QOL_LOG_ERRO, "Failed to open directory: %s\n", parent);
            return false;
        }

        struct dirent *entry;
        char full_path[QOL_PATH_BUFFER_SIZE];
        struct stat st;
        while ((entry = readdir(dir)) != NULL) {
            // Skip "." and ".." entries
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;

            if (snprintf(full_path, sizeof(full_path), "%s/%s", parent, entry->d_name) >= (int)sizeof(full_path)) {
                qol_log(QOL_LOG_WARN, "Path too long, skipping: %s/%s\n", parent, entry->d_name);
                continue;
            }

            // Check if entry is a directory
            if (stat(full_path, &st) == 0) {
                if (S_ISDIR(st.st_mode)) {
                    // Recursively read subdirectory
                    if (!qol_read_dir_recursive(full_path, content)) {
                        closedir(dir);
                        return false;
                    }
                } else {
                    // Add file to content
                    char *entry_path = strdup(full_path);
                    if (!entry_path) {
                        qol_release_string(content);
                        closedir(dir);
                        return false;
                    }
                    qol_push(content, entry_path);
                }
            } else {
                qol_log(QOL_LOG_WARN, "Failed to stat: %s\n", full_path);
            }
        }

        closedir(dir);
        return true;
#elif defined(WINDOWS)
        WIN32_FIND_DATA find_data;
        char search_path[QOL_PATH_BUFFER_SIZE];
        if (snprintf(search_path, sizeof(search_path), "%s\\*", parent) >= (int)sizeof(search_path)) {
            qol_log(QOL_LOG_ERRO, "Search path too long: %s\n", parent);
            return false;
        }

        HANDLE handle = FindFirstFile(search_path, &find_data);
        if (handle == INVALID_HANDLE_VALUE) {
            qol_log(QOL_LOG_ERRO, "Failed to open directory: %s\n", parent);
            return false;
        }

        char full_path[QOL_PATH_BUFFER_SIZE];
        do {
            // Skip "." and ".." entries
            if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0)
                continue;

            if (snprintf(full_path, sizeof(full_path), "%s\\%s", parent, find_data.cFileName) >= (int)sizeof(full_path)) {
                qol_log(QOL_LOG_WARN, "Path too long, skipping: %s\\%s\n", parent, find_data.cFileName);
                continue;
            }

            // Check if entry is a directory
            if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                // Recursively read subdirectory
                if (!qol_read_dir_recursive(full_path, content)) {
                    FindClose(handle);
                    return false;
                }
            } else {
                // Add file to content
                char *entry_path = _strdup(full_path);
                if (!entry_path) {
                    qol_release_string(content);
                    FindClose(handle);
                    return false;
                }
                qol_push(content, entry_path);
            }
        } while (FindNextFile(handle, &find_data));

        FindClose(handle);
        return true;
#else
        #error Unsupported platform
#endif
    }

    QOLDEF bool qol_read_dir(const char *parent, QOL_String *content) {
        if (!parent || !content) return false;

        content->data = NULL;
        content->len = 0;
        content->cap = 0;

#if defined(MACOS) || defined(LINUX)
        DIR *dir = opendir(parent);
        if (!dir) {
            qol_log(QOL_LOG_ERRO, "Failed to open directory: %s\n", parent);
            return false;
        }

        struct dirent *entry;
        char full_path[QOL_PATH_BUFFER_SIZE];
        while ((entry = readdir(dir)) != NULL) {
            // Skip "." and ".." entries
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;

            if (snprintf(full_path, sizeof(full_path), "%s/%s", parent, entry->d_name) >= (int)sizeof(full_path)) {
                qol_log(QOL_LOG_WARN, "Path too long, skipping: %s/%s\n", parent, entry->d_name);
                continue;
            }

            char *entry_path = strdup(full_path);
            if (!entry_path) {
                qol_release_string(content);
                closedir(dir);
                return false;
            }
            qol_push(content, entry_path);
        }

        closedir(dir);
        return true;
#elif defined(WINDOWS)
        WIN32_FIND_DATA find_data;
        char search_path[QOL_PATH_BUFFER_SIZE];
        if (snprintf(search_path, sizeof(search_path), "%s\\*", parent) >= (int)sizeof(search_path)) {
            qol_log(QOL_LOG_ERRO, "Search path too long: %s\n", parent);
            return false;
        }

        HANDLE handle = FindFirstFile(search_path, &find_data);
        if (handle == INVALID_HANDLE_VALUE) {
            qol_log(QOL_LOG_ERRO, "Failed to open directory: %s\n", parent);
            return false;
        }

        char full_path[QOL_PATH_BUFFER_SIZE];
        do {
            // Skip "." and ".." entries
            if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0)
                continue;

            if (snprintf(full_path, sizeof(full_path), "%s\\%s", parent, find_data.cFileName) >= (int)sizeof(full_path)) {
                qol_log(QOL_LOG_WARN, "Path too long, skipping: %s\\%s\n", parent, find_data.cFileName);
                continue;
            }

            char *entry_path = _strdup(full_path);
            if (!entry_path) {
                qol_release_string(content);
                FindClose(handle);
                return false;
            }
            qol_push(content, entry_path);
        } while (FindNextFile(handle, &find_data));

        FindClose(handle);
        return true;
#else
        #error Unsupported platform
#endif
    }

    QOLDEF bool qol_write_file(const char *path, const void *data, size_t size) {
        if (!path || !data) return false;

        FILE *fp = fopen(path, "wb");
        if (!fp) {
            qol_log(QOL_LOG_ERRO, "Failed to open file for writing: %s\n", path);
            return false;
        }

        size_t written = fwrite(data, 1, size, fp);
        fclose(fp);

        if (written != size) {
            qol_log(QOL_LOG_ERRO, "Failed to write all data to file: %s\n", path);
            return false;
        }

        qol_log(QOL_LOG_INFO, "Wrote %zu bytes to %s\n", written, path);
        return true;
    }

    QOLDEF const char *qol_get_file_type(const char *path) {
        if (!path) return "unknown";

        const char *dot = strrchr(path, '.');
        if (!dot || dot == path) return "no_ext";

        return dot + 1; // Returns extension without the dot
    }

    QOLDEF bool qol_delete_file(const char *path) {
        if (!path) return false;

#if defined(MACOS) || defined(LINUX)
        if (unlink(path) != 0) {
            qol_log(QOL_LOG_ERRO, "Failed to delete file: %s\n", path);
            return false;
        }

        qol_log(QOL_LOG_INFO, "Deleted file: %s\n", path);
        return true;
#elif defined(WINDOWS)
        if (DeleteFile(path) == 0) {
            qol_log(QOL_LOG_ERRO, "Failed to delete file: %s\n", path);
            return false;
        }

        qol_log(QOL_LOG_INFO, "Deleted file: %s\n", path);
        return true;
#else
        #error Unsupported platform
#endif
    }

    QOLDEF bool qol_delete_dir(const char *path) {
        if (!path) return false;

#if defined(MACOS) || defined(LINUX)
        DIR *dir = opendir(path);
        if (!dir) {
            qol_log(QOL_LOG_ERRO, "Failed to open directory for deletion: `%s`.\n", path);
            qol_log(QOL_LOG_WARN, "  Directory may not exist or is not accessible.\n");
            return false;
        }

        struct dirent *entry;
        char full_path[QOL_PATH_BUFFER_SIZE];

        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;

            if (snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name) >= (int)sizeof(full_path)) {
                qol_log(QOL_LOG_ERRO, "Path too long: %s/%s\n", path, entry->d_name);
                closedir(dir);
                return false;
            }

            struct stat st;
            if (stat(full_path, &st) == 0) {
                if (S_ISDIR(st.st_mode)) {
                    qol_delete_dir(full_path);
                } else if (S_ISREG(st.st_mode)) {
                    qol_delete_file(full_path);
                }
            }
        }

        closedir(dir);
        if (rmdir(path) != 0) {
            qol_log(QOL_LOG_ERRO, "Failed to remove directory: %s\n", path);
        } else {
            qol_log(QOL_LOG_INFO, "Removed directory: %s\n", path);
        }
        return true;
#elif defined(WINDOWS)
        WIN32_FIND_DATA find_data;
        char search_path[QOL_PATH_BUFFER_SIZE];
        if (snprintf(search_path, sizeof(search_path), "%s\\*", path) >= (int)sizeof(search_path)) {
            qol_log(QOL_LOG_ERRO, "Search path too long: %s\n", path);
            return false;
        }

        HANDLE handle = FindFirstFile(search_path, &find_data);
        if (handle == INVALID_HANDLE_VALUE) {
            qol_log(QOL_LOG_ERRO, "Failed to open directory for deletion: `%s`.\n", path);
            qol_log(QOL_LOG_WARN, "  Directory may not exist or is not accessible.\n");
            return false;
        }

        do {
            if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0)
                continue;

            char full_path[QOL_PATH_BUFFER_SIZE];
            if (snprintf(full_path, sizeof(full_path), "%s\\%s", path, find_data.cFileName) >= (int)sizeof(full_path)) {
                qol_log(QOL_LOG_ERRO, "Path too long: %s\\%s\n", path, find_data.cFileName);
                FindClose(handle);
                return false;
            }

            if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                qol_delete_dir(full_path);
            } else {
                qol_delete_file(full_path);
            }
        } while (FindNextFile(handle, &find_data));

        FindClose(handle);
        if (RemoveDirectory(path) == 0) {
            qol_log(QOL_LOG_ERRO, "Failed to remove directory: %s\n", path);
        } else {
            qol_log(QOL_LOG_INFO, "Removed directory: %s\n", path);
        }
        return true;
#else
        #error Unsupported platform
#endif

    }

    bool qol_get_files_in_dir(const char *dir_path, QOL_String *files) {
        if (!dir_path || !files) return false;

        files->data = NULL;
        files->len = 0;
        files->cap = 0;

#if defined(MACOS) || defined(LINUX)
        DIR *dir = opendir(dir_path);
        if (!dir) {
            qol_log(QOL_LOG_ERRO, "Failed to open directory: %s\n", dir_path);
            return false;
        }

        struct dirent *entry;
        char full_path[QOL_PATH_BUFFER_SIZE];
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;

            if (snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name) >= (int)sizeof(full_path)) {
                qol_log(QOL_LOG_WARN, "Path too long, skipping: %s/%s\n", dir_path, entry->d_name);
                continue;
            }

            char *file_path = strdup(full_path);
            if (!file_path) {
                qol_release_string(files);
                closedir(dir);
                return false;
            }
            qol_push(files, file_path);
        }

        closedir(dir);
        return true;
#elif defined(WINDOWS)
        WIN32_FIND_DATA find_data;
        char search_path[QOL_PATH_BUFFER_SIZE];
        if (snprintf(search_path, sizeof(search_path), "%s\\*", dir_path) >= (int)sizeof(search_path)) {
            qol_log(QOL_LOG_ERRO, "Search path too long: %s\n", dir_path);
            return false;
        }

        HANDLE handle = FindFirstFile(search_path, &find_data);
        if (handle == INVALID_HANDLE_VALUE) {
            qol_log(QOL_LOG_ERRO, "Failed to open directory: %s\n", dir_path);
            return false;
        }

        char full_path[QOL_PATH_BUFFER_SIZE];
        do {
            if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0)
                continue;

            if (snprintf(full_path, sizeof(full_path), "%s\\%s", dir_path, find_data.cFileName) >= (int)sizeof(full_path)) {
                qol_log(QOL_LOG_WARN, "Path too long, skipping: %s\\%s\n", dir_path, find_data.cFileName);
                continue;
            }

            char *file_path = _strdup(full_path);
            if (!file_path) {
                qol_release_string(files);
                FindClose(handle);
                return false;
            }
            qol_push(files, file_path);
        } while (FindNextFile(handle, &find_data));

        FindClose(handle);
        return true;
#else
        #error Unsupported platform
#endif
    }

    QOLDEF void qol_release_string(QOL_String* content) {
        if (!content || !content->data) return;

        for (size_t i = 0; i < content->len; i++) {
            free(content->data[i]);
        }
        free(content->data);
        content->data = NULL;
        content->len = content->cap = 0;
    }

    // String utilities
    QOLDEF bool qol_str_starts_with(const char *str, const char *prefix) {
        if (!str || !prefix) return false;
        size_t str_len = strlen(str);
        size_t prefix_len = strlen(prefix);
        if (prefix_len > str_len) return false;
        return strncmp(str, prefix, prefix_len) == 0;
    }

    QOLDEF bool qol_str_ends_with(const char *str, const char *suffix) {
        if (!str || !suffix) return false;
        size_t str_len = strlen(str);
        size_t suffix_len = strlen(suffix);
        if (suffix_len > str_len) return false;
        return strncmp(str + str_len - suffix_len, suffix, suffix_len) == 0;
    }

    QOLDEF char *qol_str_ltrim(char *str) {
        if (!str) return str;
        char *start = str;
        while (*start && isspace((unsigned char)*start)) {
            start++;
        }
        if (start != str) {
            size_t len = strlen(start);
            memmove(str, start, len + 1);
        }
        return str;
    }

    QOLDEF char *qol_str_rtrim(char *str) {
        if (!str) return str;
        char *end = str + strlen(str);
        while (end > str && isspace((unsigned char)*(end - 1))) {
            end--;
        }
        *end = '\0';
        return str;
    }

    QOLDEF char *qol_str_trim(char *str) {
        qol_str_rtrim(str);
        return qol_str_ltrim(str);
    }

    QOLDEF char *qol_str_replace(const char *str, const char *old_sub, const char *new_sub) {
        if (!str || !old_sub || !new_sub) return NULL;
        if (strlen(old_sub) == 0) {
            char *result = (char*)malloc(strlen(str) + 1);
            if (!result) return NULL;
            strcpy(result, str);
            return result;
        }

        size_t str_len = strlen(str);
        size_t old_len = strlen(old_sub);
        size_t new_len = strlen(new_sub);

        // Count occurrences
        size_t count = 0;
        const char *pos = str;
        while ((pos = strstr(pos, old_sub)) != NULL) {
            count++;
            pos += old_len;
        }

        // Calculate result size
        size_t result_len = str_len + count * (new_len - old_len);
        char *result = (char*)malloc(result_len + 1);
        if (!result) return NULL;

        // Build result string
        char *dst = result;
        const char *src = str;
        while (*src) {
            if (strncmp(src, old_sub, old_len) == 0) {
                memcpy(dst, new_sub, new_len);
                dst += new_len;
                src += old_len;
            } else {
                *dst++ = *src++;
            }
        }
        *dst = '\0';

        return result;
    }

    QOLDEF bool qol_str_split(const char *str, char delimiter, QOL_String *result) {
        if (!str || !result) return false;

        result->data = NULL;
        result->len = 0;
        result->cap = 0;

        const char *start = str;
        const char *end = str;

        while (*end) {
            if (*end == delimiter) {
                size_t len = end - start;
                char *token = (char*)malloc(len + 1);
                if (!token) {
                    qol_release_string(result);
                    return false;
                }
                memcpy(token, start, len);
                token[len] = '\0';
                qol_push(result, token);
                start = end + 1;
            }
            end++;
        }

        // Add final token
        size_t len = end - start;
        char *token = (char*)malloc(len + 1);
        if (!token) {
            qol_release_string(result);
            return false;
        }
        memcpy(token, start, len);
        token[len] = '\0';
        qol_push(result, token);

        return true;
    }

    QOLDEF char *qol_str_join(QOL_String *strings, const char *separator) {
        if (!strings || !separator) return NULL;

        if (strings->len == 0 || !strings->data) {
            char *result = (char*)malloc(1);
            if (!result) return NULL;
            result[0] = '\0';
            return result;
        }

        size_t sep_len = strlen(separator);
        size_t total_len = 0;

        // Calculate total length
        for (size_t i = 0; i < strings->len; i++) {
            if (strings->data[i]) {
                total_len += strlen(strings->data[i]);
            }
            if (i < strings->len - 1) {
                total_len += sep_len;
            }
        }

        char *result = (char*)malloc(total_len + 1);
        if (!result) return NULL;

        char *dst = result;
        for (size_t i = 0; i < strings->len; i++) {
            if (strings->data[i]) {
                size_t len = strlen(strings->data[i]);
                memcpy(dst, strings->data[i], len);
                dst += len;
            }
            if (i < strings->len - 1) {
                memcpy(dst, separator, sep_len);
                dst += sep_len;
            }
        }
        *dst = '\0';

        return result;
    }

    QOLDEF bool qol_str_contains(const char *str, const char *substring) {
        if (!str || !substring) return false;
        if (strlen(substring) == 0) return false;
        return strstr(str, substring) != NULL;
    }

    QOLDEF int qol_str_icmp(const char *str1, const char *str2) {
        if (!str1 || !str2) {
            if (!str1 && !str2) return 0;
            return str1 ? 1 : -1;
        }

        while (*str1 && *str2) {
            int c1 = tolower((unsigned char)*str1);
            int c2 = tolower((unsigned char)*str2);
            if (c1 != c2) {
                return c1 - c2;
            }
            str1++;
            str2++;
        }

        return tolower((unsigned char)*str1) - tolower((unsigned char)*str2);
    }

    // Path utilities
    QOLDEF const char *qol_path_name(const char *path) {
#ifdef WINDOWS
        const char *p1 = strrchr(path, '/');
        const char *p2 = strrchr(path, '\\');
        const char *p = (p1 > p2)? p1 : p2;
        return p ? p + 1 : path;
#else
        const char *p = strrchr(path, '/');
        return p ? p + 1 : path;
#endif
    }

    QOLDEF bool qol_rename(const char *old_path, const char *new_path) {
        qol_log(QOL_LOG_INFO, "renaming %s -> %s\n", old_path, new_path);
#ifdef WINDOWS
        if (!MoveFileEx(old_path, new_path, MOVEFILE_REPLACE_EXISTING)) {
            qol_log(QOL_LOG_ERRO, "could not rename %s to %s: %s\n", old_path, new_path, qol_win32_error_message(GetLastError()));
            return false;
        }
#else
        if (rename(old_path, new_path) < 0) {
            qol_log(QOL_LOG_ERRO, "could not rename %s to %s: %s\n", old_path, new_path, strerror(errno));
            return false;
        }
#endif
        return true;
    }

    QOLDEF const char *qol_get_current_dir_temp(void) {
#ifdef WINDOWS
        DWORD nBufferLength = GetCurrentDirectory(0, NULL);
        if (nBufferLength == 0) {
            qol_log(QOL_LOG_ERRO, "could not get current directory: %s\n", qol_win32_error_message(GetLastError()));
            return NULL;
        }

        char *buffer = (char*) qol_temp_alloc(nBufferLength);
        if (GetCurrentDirectory(nBufferLength, buffer) == 0) {
            qol_log(QOL_LOG_ERRO, "could not get current directory: %s\n", qol_win32_error_message(GetLastError()));
            return NULL;
        }

        return buffer;
#else
        char *buffer = (char*) qol_temp_alloc(PATH_MAX);
        if (getcwd(buffer, PATH_MAX) == NULL) {
            qol_log(QOL_LOG_ERRO, "could not get current directory: %s\n", strerror(errno));
            return NULL;
        }

        return buffer;
#endif
    }

    QOLDEF bool qol_set_current_dir(const char *path) {
#ifdef WINDOWS
        if (!SetCurrentDirectory(path)) {
            qol_log(QOL_LOG_ERRO, "could not set current directory to %s: %s\n", path, qol_win32_error_message(GetLastError()));
            return false;
        }
        return true;
#else
        if (chdir(path) < 0) {
            qol_log(QOL_LOG_ERRO, "could not set current directory to %s: %s\n", path, strerror(errno));
            return false;
        }
        return true;
#endif
    }

    QOLDEF bool qol_file_exists(const char *file_path) {
#ifdef WINDOWS
        DWORD dwAttrib = GetFileAttributesA(file_path);
        return dwAttrib != INVALID_FILE_ATTRIBUTES;
#else
        struct stat statbuf;
        if (stat(file_path, &statbuf) < 0) {
            if (errno == ENOENT) return false;
            qol_log(QOL_LOG_ERRO, "Could not check if file %s exists: %s\n", file_path, strerror(errno));
            return false;
        }
        return true;
#endif
    }

    QOLDEF int qol_needs_rebuild(const char *output_path, const char **input_paths, size_t input_paths_count) {
#ifdef WINDOWS
        // Windows: Use FILETIME (100-nanosecond intervals since 1601) for precise comparison
        BOOL bSuccess;

        // Get output file modification time
        HANDLE output_path_fd = CreateFile(output_path, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, NULL);
        if (output_path_fd == INVALID_HANDLE_VALUE) {
            // Output doesn't exist: rebuild needed
            if (GetLastError() == ERROR_FILE_NOT_FOUND) return 1;
            qol_log(QOL_LOG_ERRO, "Could not open file %s: %s\n", output_path, qol_win32_error_message(GetLastError()));
            return -1;
        }
        FILETIME output_path_time;
        // GetFileTime: NULL for creation/access time, we only need modification time
        bSuccess = GetFileTime(output_path_fd, NULL, NULL, &output_path_time);
        CloseHandle(output_path_fd);
        if (!bSuccess) {
            qol_log(QOL_LOG_ERRO, "Could not get time of %s: %s\n", output_path, qol_win32_error_message(GetLastError()));
            return -1;
        }

        // Check each input file: if any is newer than output, rebuild needed
        for (size_t i = 0; i < input_paths_count; ++i) {
            const char *input_path = input_paths[i];
            HANDLE input_path_fd = CreateFile(input_path, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, NULL);
            if (input_path_fd == INVALID_HANDLE_VALUE) {
                qol_log(QOL_LOG_ERRO, "Could not open file %s: %s\n", input_path, qol_win32_error_message(GetLastError()));
                return -1;
            }
            FILETIME input_path_time;
            bSuccess = GetFileTime(input_path_fd, NULL, NULL, &input_path_time);
            CloseHandle(input_path_fd);
            if (!bSuccess) {
                qol_log(QOL_LOG_ERRO, "Could not get time of %s: %s\n", input_path, qol_win32_error_message(GetLastError()));
                return -1;
            }

            // CompareFileTime returns 1 if input_path_time > output_path_time (input is newer)
            if (CompareFileTime(&input_path_time, &output_path_time) == 1) return 1;
        }

        return 0; // All inputs are older than output: no rebuild needed
#else
        // Unix: Use stat() to get modification time (seconds since epoch)
        struct stat statbuf = {0};

        // Get output file modification time
        if (stat(output_path, &statbuf) < 0) {
            // Output doesn't exist: rebuild needed
            if (errno == ENOENT) return 1;
            qol_log(QOL_LOG_ERRO, "could not stat %s: %s\n", output_path, strerror(errno));
            return -1;
        }
        time_t output_path_time = statbuf.st_mtime; // Modification time (seconds since epoch)

        // Check each input file: if any is newer than output, rebuild needed
        for (size_t i = 0; i < input_paths_count; ++i) {
            const char *input_path = input_paths[i];
            if (stat(input_path, &statbuf) < 0) {
                qol_log(QOL_LOG_ERRO, "could not stat %s: %s\n", input_path, strerror(errno));
                return -1;
            }
            time_t input_path_time = statbuf.st_mtime;
            // Simple integer comparison: newer files have larger timestamps
            if (input_path_time > output_path_time) return 1;
        }

        return 0; // All inputs are older than output: no rebuild needed
#endif
    }

    QOLDEF int qol_needs_rebuild1(const char *output_path, const char *input_path) {
        return qol_needs_rebuild(output_path, &input_path, 1);
    }

    //////////////////////////////////////////////////
    /// HASHMAP //////////////////////////////////////
    //////////////////////////////////////////////////

    static size_t qol_hm_hash(void* key, size_t key_size, size_t capacity) {
        // djb2 hash algorithm: hash = hash * 33 + byte
        size_t hash = 5381;
        const unsigned char *p = (const unsigned char *)key;
        for (size_t i = 0; i < key_size; i++) {
            hash = ((hash << 5) + hash) + p[i]; // hash * 33 + byte
        }
        return hash % capacity;
    }

    QOLDEF bool qol_hm_keys_equal(void* key1, void* key2) {
        // strcmp stops at the first difference or null terminator, so it never
        // reads past the end of the shorter key (unlike a fixed-size memcmp).
        return strcmp((const char*)key1, (const char*)key2) == 0;
    }

    QOLDEF QOL_HashMap* qol_hm_create() {
        QOL_HashMap* hm = (QOL_HashMap*)calloc(1, sizeof(QOL_HashMap));
        if (!hm) return NULL;

        int initial_capacity = 4;
        hm->buckets = (QOL_HashMapEntry*)calloc(initial_capacity, sizeof(QOL_HashMapEntry));
        if (!hm->buckets) {
            free(hm);
            return NULL;
        }

        hm->capacity = initial_capacity;
        hm->size = 0;
        return hm;
    }

    QOLDEF void qol_hm_resize(QOL_HashMap* hm) {
        // Save old state before allocation (needed for rollback on failure)
        size_t old_capacity = hm->capacity;
        QOL_HashMapEntry* old_buckets = hm->buckets;

        // Double the capacity (exponential growth for O(1) amortized operations)
        hm->capacity = hm->capacity * 2;
        hm->buckets = (QOL_HashMapEntry*)calloc(hm->capacity, sizeof(QOL_HashMapEntry));
        if (!hm->buckets) {
            // Allocation failed: restore old state and abort resize
            hm->buckets = old_buckets;
            hm->capacity = old_capacity;
            qol_log(QOL_LOG_ERRO, "Failed to resize hashmap\n");
            return;
        }

        // Rehash all entries: Must recompute hash indices since capacity changed
        // Hash function uses modulo capacity, so indices will change
        size_t new_size = 0;
        for (size_t i = 0; i < old_capacity; i++) {
            if (old_buckets[i].state == QOL_HM_USED) {
                // Recompute hash with new capacity (modulo operation changes)
                size_t hash = qol_hm_hash(old_buckets[i].key, old_buckets[i].key_size, hm->capacity);
                size_t index = hash;

                // Linear probing to find empty slot in new table
                // Collisions can still occur even after resize (different keys can hash to same index)
                while (hm->buckets[index].state == QOL_HM_USED) {
                    index = (index + 1) % hm->capacity; // Wrap around to start if needed
                    if (index == hash) {
                        // Wrapped all the way around: table is full (shouldn't happen with proper load factor)
                        qol_log(QOL_LOG_ERRO, "Hashmap table is full during resize\n");
                        break;
                    }
                }

                // Found empty slot: Move entry from old table to new table
                if (hm->buckets[index].state != QOL_HM_USED) {
                    // Transfer ownership: old table entries become new table entries
                    hm->buckets[index].key = old_buckets[i].key;
                    hm->buckets[index].value = old_buckets[i].value;
                    hm->buckets[index].key_size = old_buckets[i].key_size;
                    hm->buckets[index].value_size = old_buckets[i].value_size;
                    hm->buckets[index].state = QOL_HM_USED;
                    new_size++;
                }
            }
        }

        // Free old bucket array (entries were moved, not copied)
        free(old_buckets);
        hm->size = new_size; // Update size (should equal old size if all entries moved)
        qol_log(QOL_LOG_DIAG, "Hashmap resized to %zu buckets\n", hm->capacity);
    }

    QOLDEF void qol_hm_put(QOL_HashMap* hm, void* key, void* value) {
        if (!hm || !key || !value) return;

        // Keys are always strings (null-terminated)
        // Include null terminator in size for proper copying
        size_t key_size = strlen(key) + 1;

        // NOTE: Values are stored as pointers only, not copied
        // This assumes caller manages value lifetime (value must remain valid)
        // We store sizeof(void*) bytes (the pointer itself), not the data it points to
        size_t value_size = sizeof(void*);

        // Resize if load factor exceeds 0.75 (size/capacity > 0.75)
        // Prevents performance degradation from too many collisions
        // Check: size * 4 > capacity * 3  (equivalent to size/capacity > 3/4)
        if (hm->size * 4 > hm->capacity * 3) {
            qol_hm_resize(hm);
        }

        // Compute hash index: hash function + modulo for bucket selection
        size_t hash = qol_hm_hash(key, key_size, hm->capacity);
        size_t index = hash;

        // Track the first tombstone we pass, so deleted slots get reused on insert
        // (otherwise the table slowly fills with unusable DELETED entries)
        size_t first_tombstone = (size_t)-1;

        // Linear probing: Handle collisions by checking next bucket
        // Continue until we find empty slot or matching key
        while (hm->buckets[index].state != QOL_HM_EMPTY) {
            if (hm->buckets[index].state == QOL_HM_DELETED && first_tombstone == (size_t)-1) {
                first_tombstone = index;
            }
            // Check if this bucket contains our key (collision resolution)
            if (hm->buckets[index].state == QOL_HM_USED && qol_hm_keys_equal(hm->buckets[index].key, key)) {
                qol_log(QOL_LOG_DIAG, "Updating entry for key: %s\n", (const char*)key);
                // Key already exists: Update value (replace old pointer with new pointer)
                // Allocate new value storage before freeing old to avoid inconsistent state on failure
                void *new_value = malloc(value_size);
                if (new_value) {
                    // Store address of value parameter (pointer to pointer)
                    memcpy(new_value, &value, value_size);
                    free(hm->buckets[index].value); // Free old pointer storage
                    hm->buckets[index].value = new_value;
                    hm->buckets[index].value_size = value_size;
                } else {
                    // Allocation failed: log error but keep old value to maintain consistency
                    qol_log(QOL_LOG_ERRO, "Failed to allocate memory for hashmap value update\n");
                }
                return; // Update complete (or failed), no need to increment size
            }
            // Collision: Move to next bucket (wrap around if needed)
            index = (index + 1) % hm->capacity;
            if (index == hash) {
                // Wrapped all the way around without an EMPTY slot
                if (first_tombstone == (size_t)-1) {
                    qol_log(QOL_LOG_ERRO, "Hashmap table is full\n");
                    return;
                }
                break; // A tombstone is available for reuse below
            }
        }

        // Prefer reusing a tombstone over consuming a fresh empty slot
        if (first_tombstone != (size_t)-1) {
            index = first_tombstone;
        }

        // Found empty or deleted slot: Insert new entry
        if (hm->buckets[index].state == QOL_HM_EMPTY || hm->buckets[index].state == QOL_HM_DELETED) {
            qol_log(QOL_LOG_DIAG, "Inserting new entry for key: %s\n", (const char*)key);

            // Allocate memory for key and value storage
            hm->buckets[index].key = malloc(key_size);
            hm->buckets[index].value = malloc(value_size);

            // Check allocation success: Free partial allocations on failure
            if (!hm->buckets[index].key || !hm->buckets[index].value) {
                if (hm->buckets[index].key) free(hm->buckets[index].key);
                if (hm->buckets[index].value) free(hm->buckets[index].value);
                qol_log(QOL_LOG_ERRO, "Failed to allocate memory for hashmap entry\n");
                return;
            }

            // Copy key string (including null terminator)
            memcpy(hm->buckets[index].key, key, key_size);
            // Store the pointer itself, not the value it points to
            // This is why we use &value: we want to store the address of the value parameter
            memcpy(hm->buckets[index].value, &value, value_size);
            hm->buckets[index].key_size = key_size;
            hm->buckets[index].value_size = value_size;
            hm->buckets[index].state = QOL_HM_USED;
            hm->size++; // Increment entry count
        }
    }

    QOLDEF void* qol_hm_get(QOL_HashMap* hm, void* key) {
        if (!hm || !key) return NULL;

        // Compute hash index (same algorithm as qol_hm_put)
        size_t key_size = strlen(key) + 1;
        size_t hash = qol_hm_hash(key, key_size, hm->capacity);
        size_t index = hash;

        // Linear probing: Follow same collision resolution path as insertion
        // Stop when we find empty bucket (key doesn't exist) or matching key
        while (hm->buckets[index].state != QOL_HM_EMPTY) {
            // Check if this bucket contains our key
            if (hm->buckets[index].state == QOL_HM_USED && qol_hm_keys_equal(hm->buckets[index].key, key)) {
                // Found matching key: Extract stored value pointer
                // value field contains a pointer to the actual value pointer (double indirection)
                // Cast to void** to dereference and get the original pointer that was stored
                void** value_ptr = (void**)hm->buckets[index].value;
                return value_ptr ? *value_ptr : NULL; // Dereference to get actual value pointer
            }
            // Collision: Move to next bucket (same probing sequence as insertion)
            index = (index + 1) % hm->capacity;
            if (index == hash) break; // Searched entire table (wrapped around to start)
        }

        // Key not found: Empty bucket encountered or entire table searched
        return NULL;
    }

    QOLDEF bool qol_hm_contains(QOL_HashMap* hm, void* key) {
        return qol_hm_get(hm, key) != NULL ? true : false;
    }

    QOLDEF bool qol_hm_remove(QOL_HashMap* hm, void* key) {
        if (!hm || !key) return false;

        size_t key_size = strlen(key) + 1;
        size_t hash = qol_hm_hash(key, key_size, hm->capacity);
        size_t index = hash;

        // Linear probing
        while (hm->buckets[index].state != QOL_HM_EMPTY) {
            if (hm->buckets[index].state == QOL_HM_USED && qol_hm_keys_equal(hm->buckets[index].key, key)) {
                // Mark as deleted
                free(hm->buckets[index].key);
                free(hm->buckets[index].value);
                hm->buckets[index].key = NULL;
                hm->buckets[index].value = NULL;
                hm->buckets[index].state = QOL_HM_DELETED;
                hm->size--;
                return true;
            }
            index = (index + 1) % hm->capacity;
            if (index == hash) break;
        }

        return false;
    }

    QOLDEF void qol_hm_clear(QOL_HashMap* hm) {
        if (!hm) return;

        for (size_t i = 0; i < hm->capacity; i++) {
            if (hm->buckets[i].state == QOL_HM_USED) {
                free(hm->buckets[i].key);
                free(hm->buckets[i].value);
                hm->buckets[i].key = NULL;
                hm->buckets[i].value = NULL;
                hm->buckets[i].state = QOL_HM_EMPTY;
            }
        }
        hm->size = 0;
    }

    QOLDEF void qol_hm_release(QOL_HashMap* hm) {
        if (!hm) return;
        qol_hm_clear(hm);
        free(hm->buckets);
        free(hm);
    }

    QOLDEF size_t qol_hm_size(QOL_HashMap* hm) {
        return hm ? hm->size : 0;
    }

    QOLDEF bool qol_hm_empty(QOL_HashMap* hm) {
        return !hm || hm->size == 0;
    }

    //////////////////////////////////////////////////
    /// UNITTEST /////////////////////////////////////
    //////////////////////////////////////////////////

    typedef struct {
        qol_test_t tests[1024];
        size_t count;
        size_t passed;
        size_t failed;
    } qol_test_suite_t;

    qol_test_suite_t qol_test_suite = {0};
    char qol_test_failure_msg[256] = {0};

    QOLDEF void qol_test_register(const char *name, const char *file, int line, void (*test_func)(void)) {
        qol_init_mutexes();
        QOL_MUTEX_LOCK(qol_test_mutex);
        if (qol_test_suite.count >= QOL_ARRAY_LEN(qol_test_suite.tests)) {
            QOL_MUTEX_UNLOCK(qol_test_mutex);
            fprintf(stderr, "Too many tests registered!\n");
            return;
        }
        qol_test_t *test = &qol_test_suite.tests[qol_test_suite.count++];
        test->name = name;
        test->file = file;
        test->line = line;
        test->func = test_func;
        QOL_MUTEX_UNLOCK(qol_test_mutex);
    }

    QOLDEF void qol_test_fail(void) {
        qol_test_current_failed_tls = true;
    }

    QOLDEF int qol_test_run_all(void) {
        qol_init_mutexes();
        QOL_MUTEX_LOCK(qol_test_mutex);
        size_t test_count = qol_test_suite.count;
        qol_test_suite.passed = 0;
        qol_test_suite.failed = 0;
        QOL_MUTEX_UNLOCK(qol_test_mutex);

        const size_t target_width = 60;
        const char *prefix = "Testcase: ";

        // Set to true for classic three-dot output ("name ... OK") instead of
        // aligned columns of dots.
        bool legacy = false;

        for (size_t i = 0; i < test_count; i++) {
            QOL_MUTEX_LOCK(qol_test_mutex);
            qol_test_t *test = &qol_test_suite.tests[i];
            QOL_MUTEX_UNLOCK(qol_test_mutex);
            qol_test_current_failed_tls = false;
            QOL_MUTEX_LOCK(qol_test_mutex);
            qol_test_failure_msg[0] = '\0'; // Reset failure message
            QOL_MUTEX_UNLOCK(qol_test_mutex);

            // Calculate dots needed to reach alignment point.
            // Use signed math so names longer than the target width can't
            // underflow into a huge unsigned count (previously looped ~forever).
            size_t name_len = strlen(test->name);
            size_t total_prefix = strlen(prefix) + name_len;
            size_t dots_needed = (total_prefix < target_width) ? (target_width - total_prefix) : 3;

            qol_log(QOL_LOG_HINT, "%s%s ", prefix, test->name);

            // Print dots for alignment (stderr, same stream as the logger, so
            // the name, dots and result stay on one line instead of interleaving)
            for (size_t j = 0; j < dots_needed; j++) {
                if (legacy && j == 3) break;
                if (qol_logger_color) fprintf(stderr, QOL_FG_BBLACK "." QOL_RESET);
                else fprintf(stderr, ".");
            }

            // Run the test
            test->func();

            // Print result on same line with colors
            bool failed = qol_test_current_failed_tls;
            QOL_MUTEX_LOCK(qol_test_mutex);
            const char *failure_msg = qol_test_failure_msg;
            if (failed) {
                if (qol_logger_color) fprintf(stderr, QOL_FG_RED" %s"QOL_RESET"\n", (legacy ? "FAIL" : "[FAIL]"));
                else fprintf(stderr, " %s\n", (legacy ? "FAIL" : "[FAILED]"));
                if (failure_msg[0] != '\0') {
                    fprintf(stderr, "  %s\n", failure_msg);
                }
                qol_test_suite.failed++;
            } else {
                if (qol_logger_color) fprintf(stderr, QOL_FG_GREEN" %s"QOL_RESET"\n", (legacy ? "OK" : "[OK]"));
                else fprintf(stderr, " %s\n", (legacy ? "OK" : "[OK]"));
                qol_test_suite.passed++;
            }
            QOL_MUTEX_UNLOCK(qol_test_mutex);
        }

        QOL_MUTEX_LOCK(qol_test_mutex);
        size_t total = qol_test_suite.count;
        size_t passed = qol_test_suite.passed;
        size_t failed = qol_test_suite.failed;
        QOL_MUTEX_UNLOCK(qol_test_mutex);

        if (qol_logger_color) {
            qol_log(QOL_LOG_HINT, "Total: " QOL_FG_YELLOW "%zu" QOL_RESET", Passed: " QOL_FG_GREEN "%zu" QOL_RESET
                    ", Failed: " QOL_FG_RED "%zu" QOL_RESET "\n", total, passed, failed);
        } else {
            qol_log(QOL_LOG_INFO, "Total: %zu, Passed: %zu, Failed: %zu\n", total, passed, failed);
        }

        return failed > 0 ? 1 : 0;
    }

    QOLDEF void qol_test_print_summary(void) {
        qol_init_mutexes();
        QOL_MUTEX_LOCK(qol_test_mutex);
        size_t total = qol_test_suite.count;
        size_t passed = qol_test_suite.passed;
        size_t failed = qol_test_suite.failed;
        QOL_MUTEX_UNLOCK(qol_test_mutex);

        if (qol_logger_color) {
            qol_log(QOL_LOG_HINT, "Total: " QOL_FG_YELLOW "%zu" QOL_RESET ", Passed: " QOL_FG_GREEN "%zu" QOL_RESET
                    ", Failed: " QOL_FG_RED "%zu" QOL_RESET "\n", total, passed, failed);
        } else {
            qol_log(QOL_LOG_INFO, "Total: %zu, Passed: %zu, Failed: %zu\n", total, passed, failed);
        }
    }

    //////////////////////////////////////////////////
    /// TIMER ////////////////////////////////////////
    //////////////////////////////////////////////////

    QOLDEF void qol_timer_start(QOL_Timer *timer) {
        if (!timer) return;

#if defined(WINDOWS)
        QueryPerformanceFrequency(&timer->frequency);
        QueryPerformanceCounter(&timer->start);
#else
        clock_gettime(CLOCK_MONOTONIC, &timer->start);
#endif
    }

    QOLDEF double qol_timer_elapsed(QOL_Timer *timer) {
        if (!timer) return 0.0;

#if defined(WINDOWS)
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

    QOLDEF double qol_timer_elapsed_ms(QOL_Timer *timer) {
        return qol_timer_elapsed(timer) * 1000.0;
    }

    QOLDEF double qol_timer_elapsed_us(QOL_Timer *timer) {
        return qol_timer_elapsed(timer) * 1000000.0;
    }

    QOLDEF uint64_t qol_timer_elapsed_ns(QOL_Timer *timer) {
        if (!timer) return EXIT_SUCCESS;

#if defined(WINDOWS)
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

    QOLDEF void qol_timer_reset(QOL_Timer *timer) {
        if (!timer) return;
        qol_timer_start(timer);
    }

#endif // QOL_IMPLEMENTATION

//////////////////////////////////////////////////
/// QOL_STRIP_PREFIX /////////////////////////////
//////////////////////////////////////////////////

#ifdef QOL_STRIP_PREFIX

    // HELPER
    #define ASSERT                  QOL_ASSERT
    #define UNUSED                  QOL_UNUSED
    #define TODO                    QOL_TODO
    #define UNREACHABLE             QOL_UNREACHABLE
    #define ARRAY_LEN               QOL_ARRAY_LEN
    #define ARRAY_GET               QOL_ARRAY_GET

    // LOGGER
    #define init_logger             qol_init_logger
    #define init_logger_logfile     qol_init_logger_logfile
    #define get_time                qol_get_time
    #define expand_path             qol_expand_path
    #define TIME                    QOL_TIME
    #define DATE                    QOL_DATE
    #define DATETIME                QOL_DATETIME
    #define diag                    qol_diag
    #define info                    qol_info
    #define exec                    qol_exec
    #define hint                    qol_hint
    #define warn                    qol_warn
    #define erro                    qol_erro
    #define dead                    qol_dead
    #define LOG_NONE                QOL_LOG_NONE
    #define LOG_DIAG                QOL_LOG_DIAG
    #define LOG_INFO                QOL_LOG_INFO
    #define LOG_EXEC                QOL_LOG_EXEC
    #define LOG_HINT                QOL_LOG_HINT
    #define LOG_WARN                QOL_LOG_WARN
    #define LOG_ERRO                QOL_LOG_ERRO
    #define LOG_DEAD                QOL_LOG_DEAD

    // CLI_PARSER
    #define init_argparser          qol_init_argparser
    #define add_argument            qol_add_argument
    #define get_argument            qol_get_argument
    #define shift                   qol_shift
    #define arg_t                   qol_arg_t

    // NO_BUILD
    #define CmdTask                 QOL_CmdTask
    #define Proc                    QOL_Proc
    #define INVALID_PROC            QOL_INVALID_PROC
    #define auto_rebuild            qol_auto_rebuild
    #define auto_rebuild_plus       qol_auto_rebuild_plus
    #define get_filename_no_ext     qol_get_filename_no_ext
    #define default_compiler_flags  qol_default_compiler_flags
    #define default_c_build_extended qol_default_c_build_extended
    #define default_c_build         qol_default_c_build
    #define run                     qol_run
    #define run_always              qol_run_always
    #define proc_wait               qol_proc_wait
    #define procs_wait              qol_procs_wait
    #define Cmd                     QOL_Cmd
    #define Procs                   QOL_Procs
    #define RunOptions              QOL_RunOptions

    // DYN_ARRAY
    #define grow                    qol_grow
    #define shrink                  qol_shrink
    #define push                    qol_push
    #define drop                    qol_drop
    #define dropn                   qol_dropn
    #define resize                  qol_resize
    #define release                 qol_release
    #define back                    qol_back
    #define swap                    qol_swap
    #define list                    qol_list

    // FILE_OPS
    #define String                  QOL_String
    #define mkdir                   qol_mkdir
    #define mkdir_if_not_exists     qol_mkdir_if_not_exists
    #define copy_file               qol_copy_file
    #define copy_dir_rec            qol_copy_dir_rec
    #define read_dir                qol_read_dir
    #define read_dir_recursive      qol_read_dir_recursive
    #define read_file               qol_read_file
    #define write_file              qol_write_file
    #define get_file_type           qol_get_file_type
    #define delete_file             qol_delete_file
    #define delete_dir              qol_delete_dir
    #define get_files_in_dir        qol_get_files_in_dir
    #define release_string          qol_release_string
    #define path_name               qol_path_name
    #define rename                  qol_rename
    #define get_current_dir_temp    qol_get_current_dir_temp
    #define set_current_dir         qol_set_current_dir
    #define file_exists             qol_file_exists

    // STRING_UTILS
    #define str_starts_with         qol_str_starts_with
    #define str_ends_with           qol_str_ends_with
    #define str_trim                qol_str_trim
    #define str_ltrim               qol_str_ltrim
    #define str_rtrim               qol_str_rtrim
    #define str_replace             qol_str_replace
    #define str_split               qol_str_split
    #define str_join                qol_str_join
    #define str_contains            qol_str_contains
    #define str_icmp                qol_str_icmp
    #define needs_rebuild           qol_needs_rebuild
    #define needs_rebuild1          qol_needs_rebuild1

    // TEMP_ALLOCATOR
    #define temp_strdup             qol_temp_strdup
    #define temp_alloc              qol_temp_alloc
    #define temp_sprintf            qol_temp_sprintf
    #define temp_reset              qol_temp_reset
    #define temp_save               qol_temp_save
    #define temp_rewind             qol_temp_rewind

    // AUTO_FREE
    #define AUTO_FREE               QOL_AUTO_FREE

    // HASHMAP
    #define HashMap                 QOL_HashMap
    #define HashMapEntry            QOL_HashMapEntry
    #define hm_create               qol_hm_create
    #define hm_put                  qol_hm_put
    #define hm_get                  qol_hm_get
    #define hm_contains             qol_hm_contains
    #define hm_remove               qol_hm_remove
    #define hm_clear                qol_hm_clear
    #define hm_release              qol_hm_release
    #define hm_size                 qol_hm_size
    #define hm_empty                qol_hm_empty

    // UNITTEST
    #define Test                    qol_test_t
    #define test_register           qol_test_register
    #define test_run_all            qol_test_run_all
    #define test_print_summary      qol_test_print_summary
    #define TEST_ASSERT             QOL_TEST_ASSERT
    #define TEST_EQ                 QOL_TEST_EQ
    #define TEST_NEQ                QOL_TEST_NEQ
    #define TEST_STREQ              QOL_TEST_STREQ
    #define TEST_STRNEQ             QOL_TEST_STRNEQ
    #define TEST_TRUTHY             QOL_TEST_TRUTHY
    #define TEST_FALSY              QOL_TEST_FALSY
    #define TEST                    QOL_TEST

    // TIMER
    #define Timer                   QOL_Timer
    #define timer_start             qol_timer_start
    #define timer_elapsed           qol_timer_elapsed
    #define timer_elapsed_ms        qol_timer_elapsed_ms
    #define timer_elapsed_us        qol_timer_elapsed_us
    #define timer_elapsed_ns        qol_timer_elapsed_ns
    #define timer_reset             qol_timer_reset

    // ANSI COLORS
    #define enable_ansi             QOL_enable_ansi
    #define RESET                   QOL_RESET
    #define RESET_FG                QOL_RESET_FG
    #define RESET_BG                QOL_RESET_BG
    #define BOLD                    QOL_BOLD
    #define DIM                     QOL_DIM
    #define ITALIC                  QOL_ITALIC
    #define UNDERLINE               QOL_UNDERLINE
    #define INVERT                  QOL_INVERT
    #define HIDE                    QOL_HIDE
    #define STRIKE                  QOL_STRIKE
    #define FG_BLACK                QOL_FG_BLACK
    #define FG_RED                  QOL_FG_RED
    #define FG_GREEN                QOL_FG_GREEN
    #define FG_YELLOW               QOL_FG_YELLOW
    #define FG_BLUE                 QOL_FG_BLUE
    #define FG_MAGENTA              QOL_FG_MAGENTA
    #define FG_CYAN                 QOL_FG_CYAN
    #define FG_WHITE                QOL_FG_WHITE
    #define FG_BBLACK               QOL_FG_BBLACK
    #define FG_BRED                 QOL_FG_BRED
    #define FG_BGREEN               QOL_FG_BGREEN
    #define FG_BYELLOW              QOL_FG_BYELLOW
    #define FG_BBLUE                QOL_FG_BBLUE
    #define FG_BMAGENTA             QOL_FG_BMAGENTA
    #define FG_BCYAN                QOL_FG_BCYAN
    #define FG_BWHITE               QOL_FG_BWHITE
    #define BG_BLACK                QOL_BG_BLACK
    #define BG_RED                  QOL_BG_RED
    #define BG_GREEN                QOL_BG_GREEN
    #define BG_YELLOW               QOL_BG_YELLOW
    #define BG_BLUE                 QOL_BG_BLUE
    #define BG_MAGENTA              QOL_BG_MAGENTA
    #define BG_CYAN                 QOL_BG_CYAN
    #define BG_WHITE                QOL_BG_WHITE
    #define BG_BBLACK               QOL_BG_BBLACK
    #define BG_BRED                 QOL_BG_BRED
    #define BG_BGREEN               QOL_BG_BGREEN
    #define BG_BYELLOW              QOL_BG_BYELLOW
    #define BG_BBLUE                QOL_BG_BBLUE
    #define BG_BMAGENTA             QOL_BG_BMAGENTA
    #define BG_BCYAN                QOL_BG_BCYAN
    #define BG_BWHITE               QOL_BG_BWHITE
    #define FG256(n)                QOL_FG256(n)
    #define BG256(n)                QOL_BG256(n)
    #define FG_RGB(r,g,b)           QOL_FG_RGB(r,g,b)
    #define BG_RGB(r,g,b)           QOL_BG_RGB(r,g,b)

    // Platform detection
    #define is_windows              qol_is_windows
    #define is_macos                qol_is_macos
    #define is_linux                qol_is_linux
    #define os_name                 qol_os_name

#endif // QOL_STRIP_PREFIX

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // QOL_BUILD_H
