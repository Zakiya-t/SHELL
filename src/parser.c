#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"

static void init_simple_command(SimpleCommand *cmd)
{
    cmd->argv = NULL;
    cmd->argc = 0;
    cmd->input_file = NULL;
    cmd->output_file = NULL;
    cmd->append_output = false;
}

static int add_arg_to_command(SimpleCommand *cmd, int *capacity, const char *arg)
{
    if (cmd->argc + 1 >= *capacity) {
        *capacity = (*capacity == 0) ? 8 : (*capacity * 2);
        char **new_argv = realloc(cmd->argv, sizeof(char *) * (*capacity));
        if (!new_argv) {
            perror("myshell: realloc");
            return -1;
        }
        cmd->argv = new_argv;
    }

    cmd->argv[cmd->argc] = strdup(arg);
    if (!cmd->argv[cmd->argc]) {
        perror("myshell: strdup");
        return -1;
    }
    cmd->argc++;
    cmd->argv[cmd->argc] = NULL;
    return 0;
}

int parser_parse_pipeline(const TokenList *tokens, Pipeline *pipeline)
{
    pipeline->commands = NULL;
    pipeline->command_count = 0;

    if (!tokens || tokens->count == 0)
        return -1;

    /* Count pipe symbols to determine number of commands */
    int pipes = 0;
    for (int i = 0; i < tokens->count; i++) {
        if (strcmp(tokens->items[i], "|") == 0) {
            pipes++;
            if (i == 0 || i == tokens->count - 1) {
                fprintf(stderr, "myshell: syntax error near unexpected token '|'\n");
                return -1;
            }
        }
    }

    int num_cmds = pipes + 1;
    pipeline->commands = calloc(num_cmds, sizeof(SimpleCommand));
    if (!pipeline->commands) {
        perror("myshell: calloc");
        return -1;
    }
    pipeline->command_count = num_cmds;

    for (int i = 0; i < num_cmds; i++) {
        init_simple_command(&pipeline->commands[i]);
    }

    int cmd_idx = 0;
    int arg_cap = 8;
    pipeline->commands[cmd_idx].argv = malloc(sizeof(char *) * arg_cap);
    pipeline->commands[cmd_idx].argv[0] = NULL;

    for (int i = 0; i < tokens->count; i++) {
        const char *t = tokens->items[i];

        if (strcmp(t, "|") == 0) {
            if (pipeline->commands[cmd_idx].argc == 0) {
                fprintf(stderr, "myshell: syntax error near unexpected token '|'\n");
                parser_free_pipeline(pipeline);
                return -1;
            }
            cmd_idx++;
            arg_cap = 8;
            pipeline->commands[cmd_idx].argv = malloc(sizeof(char *) * arg_cap);
            pipeline->commands[cmd_idx].argv[0] = NULL;
        } else if (strcmp(t, "<") == 0) {
            if (i + 1 >= tokens->count || tokens->items[i + 1][0] == '|' ||
                tokens->items[i + 1][0] == '<' || tokens->items[i + 1][0] == '>') {
                fprintf(stderr, "myshell: syntax error near unexpected token 'newline'\n");
                parser_free_pipeline(pipeline);
                return -1;
            }
            free(pipeline->commands[cmd_idx].input_file);
            pipeline->commands[cmd_idx].input_file = strdup(tokens->items[++i]);
        } else if (strcmp(t, ">") == 0) {
            if (i + 1 >= tokens->count || tokens->items[i + 1][0] == '|' ||
                tokens->items[i + 1][0] == '<' || tokens->items[i + 1][0] == '>') {
                fprintf(stderr, "myshell: syntax error near unexpected token 'newline'\n");
                parser_free_pipeline(pipeline);
                return -1;
            }
            free(pipeline->commands[cmd_idx].output_file);
            pipeline->commands[cmd_idx].output_file = strdup(tokens->items[++i]);
            pipeline->commands[cmd_idx].append_output = false;
        } else if (strcmp(t, ">>") == 0) {
            if (i + 1 >= tokens->count || tokens->items[i + 1][0] == '|' ||
                tokens->items[i + 1][0] == '<' || tokens->items[i + 1][0] == '>') {
                fprintf(stderr, "myshell: syntax error near unexpected token 'newline'\n");
                parser_free_pipeline(pipeline);
                return -1;
            }
            free(pipeline->commands[cmd_idx].output_file);
            pipeline->commands[cmd_idx].output_file = strdup(tokens->items[++i]);
            pipeline->commands[cmd_idx].append_output = true;
        } else {
            if (add_arg_to_command(&pipeline->commands[cmd_idx], &arg_cap, t) != 0) {
                parser_free_pipeline(pipeline);
                return -1;
            }
        }
    }

    /* Verify that every command has at least one argument */
    for (int i = 0; i < num_cmds; i++) {
        if (pipeline->commands[i].argc == 0) {
            fprintf(stderr, "myshell: syntax error: empty command in pipeline\n");
            parser_free_pipeline(pipeline);
            return -1;
        }
    }

    return 0;
}

void parser_free_pipeline(Pipeline *pipeline)
{
    if (!pipeline || !pipeline->commands)
        return;

    for (int i = 0; i < pipeline->command_count; i++) {
        SimpleCommand *cmd = &pipeline->commands[i];
        if (cmd->argv) {
            for (int j = 0; j < cmd->argc; j++) {
                free(cmd->argv[j]);
            }
            free(cmd->argv);
            cmd->argv = NULL;
        }
        free(cmd->input_file);
        cmd->input_file = NULL;
        free(cmd->output_file);
        cmd->output_file = NULL;
    }

    free(pipeline->commands);
    pipeline->commands = NULL;
    pipeline->command_count = 0;
}
