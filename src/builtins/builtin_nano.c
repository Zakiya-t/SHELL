#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include "builtins.h"
#include "ui.h"

#ifndef _WIN32
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#define CTRL_KEY(k) ((k) & 0x1f)

enum EditorKey {
    KEY_BACKSPACE = 127,
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    DEL_KEY,
    HOME_KEY,
    END_KEY
};

typedef struct {
    char *chars;
    int size;
} EditorRow;

typedef struct {
    int cx, cy;            /* Cursor x and y in document */
    int row_offset;        /* Scroll row offset */
    int col_offset;        /* Scroll col offset */
    int screen_rows;
    int screen_cols;
    int num_rows;
    EditorRow *rows;
    bool dirty;
    char filename[256];
    char status_msg[128];
    struct termios orig_termios;
} EditorConfig;

static EditorConfig E;

static void disable_raw_mode(void)
{
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios);
    /* Switch back to main screen buffer and show cursor */
    printf("\033[?1049l\033[?25h");
    fflush(stdout);
}

static void enable_raw_mode(void)
{
    tcgetattr(STDIN_FILENO, &E.orig_termios);
    struct termios raw = E.orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

    /* Switch to alternate screen buffer */
    printf("\033[?1049h\033[H");
    fflush(stdout);
}

static int get_window_size(int *rows, int *cols)
{
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        *rows = 24;
        *cols = 80;
        return 0;
    }
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
}

static int editor_read_key(void)
{
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN)
            return '\0';
    }

    if (c == '\033') {
        char seq[3];
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\033';
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\033';

        if (seq[0] == '[') {
            if (seq[1] >= '0' && seq[1] <= '9') {
                if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\033';
                if (seq[2] == '~' && seq[1] == '3') return DEL_KEY;
            } else {
                switch (seq[1]) {
                    case 'A': return ARROW_UP;
                    case 'B': return ARROW_DOWN;
                    case 'C': return ARROW_RIGHT;
                    case 'D': return ARROW_LEFT;
                    case 'H': return HOME_KEY;
                    case 'F': return END_KEY;
                }
            }
        }
        return '\033';
    }
    return c;
}

static void editor_insert_row(int at, const char *s, size_t len)
{
    if (at < 0 || at > E.num_rows) return;
    E.rows = realloc(E.rows, sizeof(EditorRow) * (E.num_rows + 1));
    memmove(&E.rows[at + 1], &E.rows[at], sizeof(EditorRow) * (E.num_rows - at));

    E.rows[at].size = len;
    E.rows[at].chars = malloc(len + 1);
    memcpy(E.rows[at].chars, s, len);
    E.rows[at].chars[len] = '\0';
    E.num_rows++;
    E.dirty = true;
}

static void editor_free_row(EditorRow *row)
{
    free(row->chars);
    row->chars = NULL;
    row->size = 0;
}

static void editor_del_row(int at)
{
    if (at < 0 || at >= E.num_rows) return;
    editor_free_row(&E.rows[at]);
    memmove(&E.rows[at], &E.rows[at + 1], sizeof(EditorRow) * (E.num_rows - at - 1));
    E.num_rows--;
    E.dirty = true;
}

static void editor_row_insert_char(EditorRow *row, int at, int c)
{
    if (at < 0 || at > row->size) at = row->size;
    row->chars = realloc(row->chars, row->size + 2);
    memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
    row->size++;
    row->chars[at] = c;
    E.dirty = true;
}

static void editor_row_del_char(EditorRow *row, int at)
{
    if (at < 0 || at >= row->size) return;
    memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
    row->size--;
    E.dirty = true;
}

static void editor_insert_char(int c)
{
    if (E.cy == E.num_rows) {
        editor_insert_row(E.num_rows, "", 0);
    }
    editor_row_insert_char(&E.rows[E.cy], E.cx, c);
    E.cx++;
}

static void editor_insert_newline(void)
{
    if (E.cx == 0) {
        editor_insert_row(E.cy, "", 0);
    } else {
        EditorRow *row = &E.rows[E.cy];
        editor_insert_row(E.cy + 1, &row->chars[E.cx], row->size - E.cx);
        row = &E.rows[E.cy];
        row->size = E.cx;
        row->chars[row->size] = '\0';
    }
    E.cy++;
    E.cx = 0;
}

static void editor_del_char(void)
{
    if (E.cy == E.num_rows) return;
    if (E.cx == 0 && E.cy == 0) return;

    EditorRow *row = &E.rows[E.cy];
    if (E.cx > 0) {
        editor_row_del_char(row, E.cx - 1);
        E.cx--;
    } else {
        E.cx = E.rows[E.cy - 1].size;
        /* Merge current row into previous row */
        EditorRow *prev = &E.rows[E.cy - 1];
        prev->chars = realloc(prev->chars, prev->size + row->size + 1);
        memcpy(&prev->chars[prev->size], row->chars, row->size);
        prev->size += row->size;
        prev->chars[prev->size] = '\0';
        editor_del_row(E.cy);
        E.cy--;
    }
}

static void editor_open(const char *filename)
{
    strncpy(E.filename, filename, sizeof(E.filename) - 1);
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        /* New file */
        editor_insert_row(0, "", 0);
        E.dirty = false;
        snprintf(E.status_msg, sizeof(E.status_msg), "[ New File ] %s", filename);
        return;
    }

    char line[4096];
    while (fgets(line, sizeof(line), fp)) {
        int len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
            line[--len] = '\0';
        editor_insert_row(E.num_rows, line, len);
    }
    fclose(fp);

    if (E.num_rows == 0) {
        editor_insert_row(0, "", 0);
    }
    E.dirty = false;
    snprintf(E.status_msg, sizeof(E.status_msg), "[ Read %d lines from %s ]", E.num_rows, filename);
}

static void editor_save(void)
{
    FILE *fp = fopen(E.filename, "w");
    if (!fp) {
        snprintf(E.status_msg, sizeof(E.status_msg), "Cannot save! %s", strerror(errno));
        return;
    }

    for (int i = 0; i < E.num_rows; i++) {
        fprintf(fp, "%s\n", E.rows[i].chars);
    }
    fclose(fp);
    E.dirty = false;
    snprintf(E.status_msg, sizeof(E.status_msg), "[ Wrote %d lines to %s ]", E.num_rows, E.filename);
}

static void editor_scroll(void)
{
    if (E.cy < E.row_offset) {
        E.row_offset = E.cy;
    }
    if (E.cy >= E.row_offset + E.screen_rows) {
        E.row_offset = E.cy - E.screen_rows + 1;
    }
    if (E.cx < E.col_offset) {
        E.col_offset = E.cx;
    }
    if (E.cx >= E.col_offset + E.screen_cols) {
        E.col_offset = E.cx - E.screen_cols + 1;
    }
}

static void editor_refresh_screen(void)
{
    editor_scroll();
    printf("\033[?25l\033[H"); /* Hide cursor, move to top */

    /* 1. Header Bar */
    printf("\033[7m  MyShell Nano 1.0  File: %-30s %s\033[0m\r\n",
           E.filename, E.dirty ? "[Modified]" : "          ");

    /* 2. Text Canvas */
    for (int y = 0; y < E.screen_rows; y++) {
        int filerow = y + E.row_offset;
        if (filerow >= E.num_rows) {
            printf("~");
        } else {
            int len = E.rows[filerow].size - E.col_offset;
            if (len < 0) len = 0;
            if (len > E.screen_cols) len = E.screen_cols;
            if (len > 0) {
                printf("%.*s", len, &E.rows[filerow].chars[E.col_offset]);
            }
        }
        printf("\033[K\r\n");
    }

    /* 3. Status Bar */
    printf("\033[7m%-40s Line %d/%d Col %d\033[0m\r\n",
           E.status_msg, E.cy + 1, E.num_rows, E.cx + 1);

    /* 4. Command Help Bar */
    printf("\033[1m^O\033[0m WriteOut (Save)    \033[1m^X\033[0m Exit\033[K");

    /* Move cursor to active edit position */
    printf("\033[%d;%dH", (E.cy - E.row_offset) + 2, (E.cx - E.col_offset) + 1);
    printf("\033[?25h"); /* Show cursor */
    fflush(stdout);
}

static void editor_move_cursor(int key)
{
    EditorRow *row = (E.cy >= E.num_rows) ? NULL : &E.rows[E.cy];
    switch (key) {
        case ARROW_LEFT:
            if (E.cx > 0) {
                E.cx--;
            } else if (E.cy > 0) {
                E.cy--;
                E.cx = E.rows[E.cy].size;
            }
            break;
        case ARROW_RIGHT:
            if (row && E.cx < row->size) {
                E.cx++;
            } else if (row && E.cx == row->size && E.cy < E.num_rows - 1) {
                E.cy++;
                E.cx = 0;
            }
            break;
        case ARROW_UP:
            if (E.cy > 0) E.cy--;
            break;
        case ARROW_DOWN:
            if (E.cy < E.num_rows - 1) E.cy++;
            break;
        case HOME_KEY:
            E.cx = 0;
            break;
        case END_KEY:
            if (row) E.cx = row->size;
            break;
    }

    row = (E.cy >= E.num_rows) ? NULL : &E.rows[E.cy];
    int rowlen = row ? row->size : 0;
    if (E.cx > rowlen) E.cx = rowlen;
}
#endif

int builtin_nano(int argc, char **argv, ShellState *state)
{
    (void)state;

#ifndef _WIN32
    if (!isatty(STDIN_FILENO)) {
        fprintf(stderr, "myshell: nano: standard input must be a terminal\n");
        return 1;
    }

    memset(&E, 0, sizeof(E));
    get_window_size(&E.screen_rows, &E.screen_cols);
    E.screen_rows -= 3; /* Account for header bar, status bar, and help bar */
    if (E.screen_rows < 1) E.screen_rows = 1;

    const char *target_file = (argc > 1) ? argv[1] : "untitled.txt";
    editor_open(target_file);

    enable_raw_mode();

    while (1) {
        editor_refresh_screen();
        int c = editor_read_key();

        if (c == CTRL_KEY('x')) {
            break;
        } else if (c == CTRL_KEY('o')) {
            editor_save();
        } else if (c == '\r' || c == '\n') {
            editor_insert_newline();
        } else if (c == KEY_BACKSPACE || c == 8) {
            editor_del_char();
        } else if (c == DEL_KEY) {
            editor_move_cursor(ARROW_RIGHT);
            editor_del_char();
        } else if (c == ARROW_UP || c == ARROW_DOWN || c == ARROW_LEFT || c == ARROW_RIGHT ||
                   c == HOME_KEY || c == END_KEY) {
            editor_move_cursor(c);
        } else if (c >= 32 && c <= 126) {
            editor_insert_char(c);
        }
    }

    disable_raw_mode();

    /* Clean up row memory */
    for (int i = 0; i < E.num_rows; i++) {
        editor_free_row(&E.rows[i]);
    }
    free(E.rows);
    return 0;

#else
    (void)argc;
    (void)argv;
    printf("MyShell Native Nano: Interactive text editor designed for POSIX/Linux terminals.\n");
    printf("Run on Linux/WSL2 Ubuntu to use the full interactive visual editor!\n");
    return 0;
#endif
}

