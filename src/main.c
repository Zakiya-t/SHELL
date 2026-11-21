#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <sys/resource.h>
#endif
#include "shell.h"
#include "ui.h"
#include "lexer.h"
#include "parser.h"
#include "executor.h"

int main(void)
{
    ShellState state;
    shell_state_init(&state);


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

#ifndef _WIN32
        if (state.telemetry_enabled) {
            struct rusage ru;
            getrusage(RUSAGE_CHILDREN, &ru);
            printf("%s[ ⚡ Exit: %d | 💾 RAM: %ld KB | 🔄 Context Switches: %ld | 📄 Page Faults: %ld ]%s\n",
                   COLOR_CYAN, state.last_exit_code, ru.ru_maxrss,
                   ru.ru_nvcsw + ru.ru_nivcsw, ru.ru_minflt, COLOR_RESET);
        }
#endif

        lexer_free(&tokens);
    }

    int exit_code = state.last_exit_code;
    shell_state_cleanup(&state);
    return exit_code;
}
