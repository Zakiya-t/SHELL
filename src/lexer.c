#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "lexer.h"

static void add_token(TokenList *list, int *capacity, const char *token)
{
    if (list->count == *capacity) {
        *capacity *= 2;
        char **grown = realloc(list->items, sizeof(char *) * (*capacity));
        if (!grown) {
            perror("myshell: realloc");
            return;
        }
        list->items = grown;
    }

    list->items[list->count] = strdup(token);
    if (list->items[list->count]) {
        list->count++;
    }
}

TokenList lexer_tokenize(const char *input)
{
    TokenList result = {NULL, 0};
    int capacity = 8;
    result.items = malloc(sizeof(char *) * capacity);

    if (!result.items) {
        perror("myshell: malloc");
        return result;
    }

    const char *p = input;

    while (*p) {
        /* Skip leading whitespace */
        while (isspace((unsigned char)*p))
            p++;

        if (*p == '\0')
            break;

        /* Check for shell operators outside quotes */
        if (*p == '|') {
            add_token(&result, &capacity, "|");
            p++;
            continue;
        }

        if (*p == '<') {
            add_token(&result, &capacity, "<");
            p++;
            continue;
        }

        if (*p == '>') {
            if (*(p + 1) == '>') {
                add_token(&result, &capacity, ">>");
                p += 2;
            } else {
                add_token(&result, &capacity, ">");
                p++;
            }
            continue;
        }

        /* Read a word token (may contain quotes) */
        char token[2048];
        int len = 0;

        while (*p) {
            if (isspace((unsigned char)*p))
                break;

            /* Operator ends the current word if not inside quotes */
            if (*p == '|' || *p == '<' || *p == '>')
                break;

            if (*p == '\'' || *p == '"') {
                char quote = *p++;
                while (*p && *p != quote) {
                    if (len < (int)sizeof(token) - 1)
                        token[len++] = *p;
                    p++;
                }
                if (*p == quote)
                    p++; /* Skip closing quote */
            } else {
                if (len < (int)sizeof(token) - 1)
                    token[len++] = *p;
                p++;
            }
        }

        token[len] = '\0';
        if (len > 0) {
            add_token(&result, &capacity, token);
        }
    }

    return result;
}

void lexer_free(TokenList *tokens)
{
    if (!tokens || !tokens->items)
        return;

    for (int i = 0; i < tokens->count; i++) {
        free(tokens->items[i]);
    }

    free(tokens->items);
    tokens->items = NULL;
    tokens->count = 0;
}
