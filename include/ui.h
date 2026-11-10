#ifndef UI_H
#define UI_H

#include "shell.h"

/* ANSI Terminal Color & Style Codes */
#define COLOR_RESET   "\033[0m"
#define COLOR_BOLD    "\033[1m"
#define COLOR_DIM     "\033[2m"

#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_WHITE   "\033[37m"

#define COLOR_BRIGHT_BLUE    "\033[94m"
#define COLOR_BRIGHT_GREEN   "\033[92m"
#define COLOR_BRIGHT_CYAN    "\033[96m"
#define COLOR_BRIGHT_YELLOW  "\033[93m"

/* UI Rendering Functions */
void ui_render_prompt(const ShellState *state);
void ui_print_banner(void);

#endif

