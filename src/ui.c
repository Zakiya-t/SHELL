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
