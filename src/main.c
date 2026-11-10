#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "shell.h"
#include "ui.h"
#include "lexer.h"
#include "parser.h"
#include "executor.h"

int main(void)
{
    ShellState state;
    shell_state_init(&state);

    ui_print_banner();

    char input[MAX_INPUT_SIZE];

    while (state.is_running) {
        ui_render_prompt(&state);

        if (fgets(input, sizeof(input), stdin) == NULL) {
            putchar('\n');
            break;
        }

        /* Remove trailing newline characters */
        input[strcspn(input, "\r\n")] = '\0';

        if (strlen(input) == 0) {
            continue;
        }

        /* Record in session history */
        shell_state_add_history(&state, input);

        /* Tokenize */
        TokenList tokens = lexer_tokenize(input);
        if (tokens.count == 0) {
            lexer_free(&tokens);
            continue;
        }

        /* Parse into pipeline (commands, pipes, redirections) */
        Pipeline pipeline;
        if (parser_parse_pipeline(&tokens, &pipeline) == 0) {
            state.last_exit_code = executor_run_pipeline(&pipeline, &state);
            parser_free_pipeline(&pipeline);
        } else {
            state.last_exit_code = 2; /* Syntax error code */
        }

        lexer_free(&tokens);
    }

    int exit_code = state.last_exit_code;
    shell_state_cleanup(&state);
    return exit_code;
}
