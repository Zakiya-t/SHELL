#ifndef LEXER_H
#define LEXER_H

typedef struct {
    char **items;
    int count;
} TokenList;

TokenList lexer_tokenize(const char *input);
void lexer_free(TokenList *tokens);

#endif

