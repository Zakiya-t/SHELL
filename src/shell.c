#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "shell.h"

#ifdef _WIN32
#include <direct.h>
#define getcwd _getcwd
#else
#include <unistd.h>
#endif

void shell_state_init(ShellState *state)
{
    state->last_exit_code = 0;
    state->is_running = true;
    state->prev_dir[0] = '\0';
    state->history_count = 0;
    state->history_capacity = 32;
    state->history = malloc(sizeof(char *) * state->history_capacity);

    shell_state_update_cwd(state);
}

void shell_state_cleanup(ShellState *state)
{
    if (state->history) {
        for (int i = 0; i < state->history_count; i++) {
            free(state->history[i]);
        }
        free(state->history);
        state->history = NULL;
    }
    state->history_count = 0;
    state->history_capacity = 0;
}

void shell_state_add_history(ShellState *state, const char *line)
{
    if (!line || strlen(line) == 0)
        return;

    /* Don't record consecutive duplicate commands */
    if (state->history_count > 0 &&
        strcmp(state->history[state->history_count - 1], line) == 0) {
        return;
    }

    if (state->history_count == state->history_capacity) {
        state->history_capacity *= 2;
        char **new_hist = realloc(state->history, sizeof(char *) * state->history_capacity);
        if (!new_hist)
            return;
        state->history = new_hist;
    }

    state->history[state->history_count] = strdup(line);
    if (state->history[state->history_count]) {
        state->history_count++;
    }
}

void shell_state_update_cwd(ShellState *state)
{
    if (getcwd(state->current_dir, sizeof(state->current_dir)) == NULL) {
        strncpy(state->current_dir, "/", sizeof(state->current_dir));
    }
}

