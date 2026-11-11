#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ui.h"

static void format_path(const char *raw_path, char *out_path, size_t out_size)
{
    const char *home = getenv("HOME");
#ifdef _WIN32
    if (!home)
        home = getenv("USERPROFILE");
#endif

    if (home && strncmp(raw_path, home, strlen(home)) == 0) {
        snprintf(out_path, out_size, "~%s", raw_path + strlen(home));
    } else {
        snprintf(out_path, out_size, "%s", raw_path);
    }
}

void ui_render_prompt(const ShellState *state)
{
    const char *user = getenv("USER");
#ifdef _WIN32
    if (!user)
        user = getenv("USERNAME");
#endif
    if (!user)
        user = "user";

    char display_path[1024];
    format_path(state->current_dir, display_path, sizeof(display_path));

    /* User in bright cyan, path in bright yellow */
    printf("%s%s%s%s:", COLOR_BOLD, COLOR_BRIGHT_CYAN, user, COLOR_RESET);
    printf("%s%s%s%s ", COLOR_BOLD, COLOR_BRIGHT_YELLOW, display_path, COLOR_RESET);

    /* Fish-style prompt arrow */
    printf("%s%s❯%s ", COLOR_BOLD, COLOR_BRIGHT_BLUE, COLOR_RESET);
    fflush(stdout);
}

// void ui_print_banner(void)
// {
//     printf("%s%s=======================================================%s\n", COLOR_BOLD, COLOR_CYAN, COLOR_RESET);
//     printf("%s%s   __  __       ____  _          _ _                  %s\n", COLOR_BOLD, COLOR_BRIGHT_CYAN, COLOR_RESET);
//     printf("%s%s  |  \\/  |_   _/ ___|| |__   ___| | |                 %s\n", COLOR_BOLD, COLOR_BRIGHT_CYAN, COLOR_RESET);
//     printf("%s%s  | |\\/| | | | \\___ \\| '_ \\ / _ \\ | |                 %s\n", COLOR_BOLD, COLOR_BRIGHT_CYAN, COLOR_RESET);
//     printf("%s%s  | |  | | |_| |___) | | | |  __/ | |                 %s\n", COLOR_BOLD, COLOR_BRIGHT_CYAN, COLOR_RESET);
//     printf("%s%s  |_|  |_|\\__, |____/|_| |_|\\___|_|_|  v%s         %s\n", COLOR_BOLD, COLOR_BRIGHT_CYAN, SHELL_VERSION, COLOR_RESET);
//     printf("%s%s          |___/                                        %s\n", COLOR_BOLD, COLOR_BRIGHT_CYAN, COLOR_RESET);
//     printf("%s%s  A Modern, Modular Monolithic Shell                   %s\n", COLOR_DIM, COLOR_WHITE, COLOR_RESET);
//     printf("%s%s  Type %s'help'%s%s to list built-ins, or %s'sysinfo'%s%s for dashboard.%s\n", 
//            COLOR_DIM, COLOR_WHITE, COLOR_BOLD, COLOR_RESET, COLOR_DIM, COLOR_BOLD, COLOR_RESET, COLOR_DIM, COLOR_RESET);
//     printf("%s%s=======================================================%s\n\n", COLOR_BOLD, COLOR_CYAN, COLOR_RESET);
// }

