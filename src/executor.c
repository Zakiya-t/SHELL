#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include "executor.h"
#include "builtins.h"

#ifdef _WIN32
#include <io.h>
#include <process.h>
#define dup _dup
#define dup2 _dup2
#define close _close
#define open _open
#ifndef STDIN_FILENO
#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#endif
#else
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#endif

static int apply_redirections(const SimpleCommand *cmd, int *saved_stdin, int *saved_stdout)
{
    *saved_stdin = -1;
    *saved_stdout = -1;

    if (cmd->input_file) {
        *saved_stdin = dup(STDIN_FILENO);
        int fd = open(cmd->input_file, O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "myshell: %s: %s\n", cmd->input_file, strerror(errno));
            return -1;
        }
        dup2(fd, STDIN_FILENO);
        close(fd);
    }

    if (cmd->output_file) {
        *saved_stdout = dup(STDOUT_FILENO);
        int flags = O_WRONLY | O_CREAT | (cmd->append_output ? O_APPEND : O_TRUNC);
#ifdef _WIN32
        int fd = open(cmd->output_file, flags | _O_BINARY, 0666);
#else
        int fd = open(cmd->output_file, flags, 0644);
#endif
        if (fd < 0) {
            fprintf(stderr, "myshell: %s: %s\n", cmd->output_file, strerror(errno));
            if (*saved_stdin != -1) {
                dup2(*saved_stdin, STDIN_FILENO);
                close(*saved_stdin);
                *saved_stdin = -1;
            }
            return -1;
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }

    return 0;
}

static void restore_redirections(int saved_stdin, int saved_stdout)
{
    if (saved_stdin != -1) {
        dup2(saved_stdin, STDIN_FILENO);
        close(saved_stdin);
    }
    if (saved_stdout != -1) {
        dup2(saved_stdout, STDOUT_FILENO);
        close(saved_stdout);
    }
}

static int execute_single_command(const SimpleCommand *cmd, ShellState *state)
{
    if (!cmd || cmd->argc == 0 || !cmd->argv[0])
        return 0;

    int saved_stdin = -1, saved_stdout = -1;
    if (apply_redirections(cmd, &saved_stdin, &saved_stdout) != 0) {
        return 1;
    }

    int exit_code = 0;

    if (is_builtin(cmd->argv[0])) {
        exit_code = execute_builtin(cmd, state);
    } else {
#ifdef _WIN32
        intptr_t ret = _spawnvp(_P_WAIT, cmd->argv[0], (const char * const *)cmd->argv);
        if (ret == -1) {
            if (errno == ENOENT)
                fprintf(stderr, "myshell: %s: command not found\n", cmd->argv[0]);
            else
                fprintf(stderr, "myshell: %s: %s\n", cmd->argv[0], strerror(errno));
            exit_code = 127;
        } else {
            exit_code = (int)ret;
        }
#else
        pid_t pid = fork();
        if (pid < 0) {
            perror("myshell: fork");
            exit_code = 1;
        } else if (pid == 0) {
            execvp(cmd->argv[0], cmd->argv);
            if (errno == ENOENT) {
                fprintf(stderr, "myshell: %s: command not found\n", cmd->argv[0]);
                exit(127);
            } else {
                fprintf(stderr, "myshell: %s: %s\n", cmd->argv[0], strerror(errno));
                exit(126);
            }
        } else {
            int status = 0;
            waitpid(pid, &status, 0);
            if (WIFEXITED(status))
                exit_code = WEXITSTATUS(status);
            else if (WIFSIGNALED(status))
                exit_code = 128 + WTERMSIG(status);
        }
#endif
    }

    restore_redirections(saved_stdin, saved_stdout);
    return exit_code;
}

#ifndef _WIN32
static int execute_pipeline_posix(const Pipeline *pipeline, ShellState *state)
{
    int prev_pipe_read = -1;
    pid_t *pids = malloc(sizeof(pid_t) * pipeline->command_count);
    if (!pids) {
        perror("myshell: malloc");
        return 1;
    }

    for (int i = 0; i < pipeline->command_count; i++) {
        int pipefd[2];
        if (i < pipeline->command_count - 1) {
            if (pipe(pipefd) < 0) {
                perror("myshell: pipe");
                free(pids);
                return 1;
            }
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("myshell: fork");
            free(pids);
            return 1;
        }

        if (pid == 0) {
            /* Child process i */
            if (i > 0) {
                dup2(prev_pipe_read, STDIN_FILENO);
                close(prev_pipe_read);
            }

            if (i < pipeline->command_count - 1) {
                dup2(pipefd[1], STDOUT_FILENO);
                close(pipefd[1]);
                close(pipefd[0]);
            }

            int saved_in = -1, saved_out = -1;
            if (apply_redirections(&pipeline->commands[i], &saved_in, &saved_out) != 0) {
                exit(1);
            }

            if (is_builtin(pipeline->commands[i].argv[0])) {
                int status = execute_builtin(&pipeline->commands[i], state);
                exit(status);
            } else {
                execvp(pipeline->commands[i].argv[0], pipeline->commands[i].argv);
                if (errno == ENOENT) {
                    fprintf(stderr, "myshell: %s: command not found\n", pipeline->commands[i].argv[0]);
                    exit(127);
                } else {
                    fprintf(stderr, "myshell: %s: %s\n", pipeline->commands[i].argv[0], strerror(errno));
                    exit(126);
                }
            }
        }

        pids[i] = pid;

        if (i > 0) {
            close(prev_pipe_read);
        }

        if (i < pipeline->command_count - 1) {
            close(pipefd[1]);
            prev_pipe_read = pipefd[0];
        }
    }

    int last_exit_code = 0;
    for (int i = 0; i < pipeline->command_count; i++) {
        int status = 0;
        waitpid(pids[i], &status, 0);
        if (i == pipeline->command_count - 1) {
            if (WIFEXITED(status))
                last_exit_code = WEXITSTATUS(status);
            else if (WIFSIGNALED(status))
                last_exit_code = 128 + WTERMSIG(status);
        }
    }

    free(pids);
    return last_exit_code;
}
#endif

int executor_run_pipeline(const Pipeline *pipeline, ShellState *state)
{
    if (!pipeline || pipeline->command_count == 0)
        return 0;

    if (pipeline->command_count == 1) {
        return execute_single_command(&pipeline->commands[0], state);
    }

#ifndef _WIN32
    return execute_pipeline_posix(pipeline, state);
#else
    fprintf(stderr, "myshell: multi-command pipelines require Linux/WSL2 POSIX environment\n");
    return 1;
#endif
}
