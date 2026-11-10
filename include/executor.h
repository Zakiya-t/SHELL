#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "parser.h"
#include "shell.h"

/*
 * Evaluates and executes a Pipeline of commands.
 * Handles file redirections (<, >, >>) and multi-command pipelines (cmd1 | cmd2).
 * Returns the exit status of the final command in the pipeline.
 */
int executor_run_pipeline(const Pipeline *pipeline, ShellState *state);

#endif
