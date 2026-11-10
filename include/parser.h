#ifndef PARSER_H
#define PARSER_H

#include <stdbool.h>
#include "lexer.h"

/* Represents a single command with arguments and redirections */
typedef struct {
    char **argv;         /* NULL-terminated argument array */
    int argc;            /* Argument count */
    char *input_file;    /* Redirect stdin: '< file' (NULL if none) */
    char *output_file;   /* Redirect stdout: '> file' or '>> file' (NULL if none) */
    bool append_output;  /* true for '>>', false for '>' */
} SimpleCommand;

/* Represents a pipeline of one or more commands: cmd1 | cmd2 | cmd3 */
typedef struct {
    SimpleCommand *commands;
    int command_count;
} Pipeline;

int parser_parse_pipeline(const TokenList *tokens, Pipeline *pipeline);
void parser_free_pipeline(Pipeline *pipeline);

#endif
