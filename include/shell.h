#ifndef SHELL_H
#define SHELL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define SHELL_NAME "MyShell"
#define SHELL_VERSION "1.0.0"
#define MAX_INPUT_SIZE 2048
#define MAX_HISTORY_SIZE 1000

/* Global shell runtime state */
typedef struct {
    int last_exit_code;
    bool is_running;
    char current_dir[1024];
    char prev_dir[1024];
    char **history;
    int history_count;
    int history_capacity;
} ShellState;

/* Shell state lifecycle */
void shell_state_init(ShellState *state);
void shell_state_cleanup(ShellState *state);
void shell_state_add_history(ShellState *state, const char *line);
void shell_state_update_cwd(ShellState *state);

#endif

