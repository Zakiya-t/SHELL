#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "builtins.h"
#include "ui.h"

#ifdef _WIN32
#include <direct.h>
#define chdir _chdir
#define getcwd _getcwd
#else
#include <unistd.h>
#endif

static void set_env_var(const char *key, const char *value)
{
#ifdef _WIN32
    _putenv_s(key, value);
#else
    setenv(key, value, 1);
#endif
}

int builtin_cd(int argc, char **argv, ShellState *state)
{
    const char *target = NULL;

    if (argc < 2 || strcmp(argv[1], "~") == 0) {
        target = getenv("HOME");
#ifdef _WIN32
        if (!target)
            target = getenv("USERPROFILE");
#endif
        if (!target) {
            fprintf(stderr, "myshell: cd: HOME not set\n");
            return 1;
        }
    } else if (strcmp(argv[1], "-") == 0) {
        if (state->prev_dir[0] == '\0') {
            fprintf(stderr, "myshell: cd: OLDPWD not set\n");
            return 1;
        }
        target = state->prev_dir;
        printf("%s\n", target);
    } else {
        target = argv[1];
    }

    /* Save current directory as previous before changing */
    char temp_dir[1024];
    if (getcwd(temp_dir, sizeof(temp_dir)) != NULL) {
        strncpy(state->prev_dir, temp_dir, sizeof(state->prev_dir) - 1);
        state->prev_dir[sizeof(state->prev_dir) - 1] = '\0';
        set_env_var("OLDPWD", state->prev_dir);
    }

    if (chdir(target) != 0) {
        fprintf(stderr, "myshell: cd: %s: %s\n", target, strerror(errno));
        return 1;
    }

    shell_state_update_cwd(state);
    set_env_var("PWD", state->current_dir);
    return 0;
}

int builtin_pwd(int argc, char **argv, ShellState *state)
{
    (void)argc;
    (void)argv;
    printf("%s\n", state->current_dir);
    return 0;
}

int builtin_echo(int argc, char **argv, ShellState *state)
{
    (void)state;
    int start = 1;
    bool newline = true;

    if (argc > 1 && strcmp(argv[1], "-n") == 0) {
        newline = false;
        start = 2;
    }

    for (int i = start; i < argc; i++) {
        printf("%s%s", argv[i], (i + 1 < argc) ? " " : "");
    }

    if (newline)
        putchar('\n');
    else
        fflush(stdout);

    return 0;
}

int builtin_clear(int argc, char **argv, ShellState *state)
{
    (void)argc;
    (void)argv;
    (void)state;
    printf("\033[H\033[2J\033[3J");
    fflush(stdout);
    return 0;
}

int builtin_history(int argc, char **argv, ShellState *state)
{
    (void)argc;
    (void)argv;
    for (int i = 0; i < state->history_count; i++) {
        printf("%s%5d%s  %s\n", COLOR_DIM, i + 1, COLOR_RESET, state->history[i]);
    }
    return 0;
}

int builtin_type(int argc, char **argv, ShellState *state)
{
    (void)state;
    if (argc < 2) {
        fprintf(stderr, "myshell: type: missing operand\n");
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        if (is_builtin(argv[i])) {
            printf("%s is a %sMyShell built-in%s command\n", argv[i], COLOR_GREEN, COLOR_RESET);
        } else {
            printf("%s is an %sexternal system command%s (resolved via PATH)\n", argv[i], COLOR_CYAN, COLOR_RESET);
        }
    }
    return 0;
}

int builtin_exit(int argc, char **argv, ShellState *state)
{
    state->is_running = false;
    if (argc > 1) {
        state->last_exit_code = atoi(argv[1]);
    }
    return state->last_exit_code;
}

/* Builtins registry table */
static const BuiltinCommand BUILTINS[] = {
    {"cd",      builtin_cd,      "Change the current directory",          "cd [dir | ~ | -]"},
    {"pwd",     builtin_pwd,     "Print current working directory",       "pwd"},
    {"ls",      builtin_ls,      "List directory contents with colors",   "ls [-l] [-a] [path]"},
    {"cat",     builtin_cat,     "Concatenate and display files",         "cat [-n] <file...>"},
    {"mkdir",   builtin_mkdir,   "Create directory",                      "mkdir <dir...>"},
    {"touch",   builtin_touch,   "Create empty file or update timestamp", "touch <file...>"},
    {"nano",    builtin_nano,    "Interactive terminal text editor",      "nano [file]"},
    {"sysinfo", builtin_sysinfo, "Display modern system information",     "sysinfo"},
    {"lscpu",   builtin_lscpu,   "Query CPU architecture from /proc/cpuinfo", "lscpu"},
    {"top",     builtin_top,     "Live kernel process & memory monitor",      "top [-n iterations]"},
    {"echo",    builtin_echo,    "Display a line of text",                "echo [-n] [string...]"},
    {"clear",   builtin_clear,   "Clear the terminal screen",             "clear"},
    {"history", builtin_history, "Display command history",               "history"},
    {"type",    builtin_type,    "Display command classification",        "type <command>"},
    {"help",    builtin_help,    "Show help manual for built-ins",        "help"},
    {"exit",    builtin_exit,    "Exit MyShell",                          "exit [status]"},
};

static const int NUM_BUILTINS = sizeof(BUILTINS) / sizeof(BUILTINS[0]);

const BuiltinCommand *get_builtins_table(int *count)
{
    if (count)
        *count = NUM_BUILTINS;
    return BUILTINS;
}

int builtin_help(int argc, char **argv, ShellState *state)
{
    (void)argc;
    (void)argv;
    (void)state;

    printf("\n%s%sMyShell%s - A Modern Modular Monolithic Shell (v%s)\n",
           COLOR_BOLD, COLOR_BRIGHT_CYAN, COLOR_RESET, SHELL_VERSION);
    printf("Inspired by modern shells like Fish, built from scratch in C.\n\n");
    printf("%sBuilt-in Command Suite:%s\n", COLOR_BOLD, COLOR_RESET);

    for (int i = 0; i < NUM_BUILTINS; i++) {
        printf("  %s%-10s%s %-32s %s(%s)%s\n",
               COLOR_BRIGHT_GREEN, BUILTINS[i].name, COLOR_RESET,
               BUILTINS[i].description,
               COLOR_DIM, BUILTINS[i].usage, COLOR_RESET);
    }

    printf("\n%sExternal Commands:%s\n", COLOR_BOLD, COLOR_RESET);
    printf("  Any system command (%sgit%s, %sgcc%s, %spython%s, %scurl%s, etc.) is executed\n",
           COLOR_CYAN, COLOR_RESET, COLOR_CYAN, COLOR_RESET, COLOR_CYAN, COLOR_RESET, COLOR_CYAN, COLOR_RESET);
    printf("  as a separate process via POSIX fork() & execvp().\n\n");
    return 0;
}

bool is_builtin(const char *name)
{
    if (!name)
        return false;

    for (int i = 0; i < NUM_BUILTINS; i++) {
        if (strcmp(name, BUILTINS[i].name) == 0)
            return true;
    }
    return false;
}

int execute_builtin(const SimpleCommand *cmd, ShellState *state)
{
    if (!cmd || cmd->argc == 0 || !cmd->argv[0])
        return 0;

    for (int i = 0; i < NUM_BUILTINS; i++) {
        if (strcmp(cmd->argv[0], BUILTINS[i].name) == 0) {
            return BUILTINS[i].func(cmd->argc, cmd->argv, state);
        }
    }
    return -1;
}

