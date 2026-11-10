#ifndef BUILTINS_H
#define BUILTINS_H

#include <stdbool.h>
#include "shell.h"
#include "parser.h"

/* Function pointer type for built-in command handlers */
typedef int (*BuiltinFn)(int argc, char **argv, ShellState *state);

typedef struct {
    const char *name;
    BuiltinFn func;
    const char *description;
    const char *usage;
} BuiltinCommand;

/* Built-in command prototypes */
int builtin_cd(int argc, char **argv, ShellState *state);
int builtin_pwd(int argc, char **argv, ShellState *state);
int builtin_ls(int argc, char **argv, ShellState *state);
int builtin_cat(int argc, char **argv, ShellState *state);
int builtin_mkdir(int argc, char **argv, ShellState *state);
int builtin_touch(int argc, char **argv, ShellState *state);
int builtin_sysinfo(int argc, char **argv, ShellState *state);
int builtin_echo(int argc, char **argv, ShellState *state);
int builtin_clear(int argc, char **argv, ShellState *state);
int builtin_history(int argc, char **argv, ShellState *state);
int builtin_type(int argc, char **argv, ShellState *state);
int builtin_help(int argc, char **argv, ShellState *state);
int builtin_lscpu(int argc, char **argv, ShellState *state);
int builtin_top(int argc, char **argv, ShellState *state);
int builtin_nano(int argc, char **argv, ShellState *state);
int builtin_exit(int argc, char **argv, ShellState *state);

/* Builtin dispatcher interface */
bool is_builtin(const char *name);
int execute_builtin(const SimpleCommand *cmd, ShellState *state);
const BuiltinCommand *get_builtins_table(int *count);

#endif

