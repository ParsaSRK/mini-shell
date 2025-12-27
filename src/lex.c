#include "lex.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>


// Lexer Structures

static const char lex_whitespaces[] = " \n\t";
static const char lex_operators[] = ";|&<>";

// Memory Management Functions

void free_lex_token(lex_token *token) {
    if (!token) return;
    free(token->data);
    free(token);
}

void free_lex_token_adapter(void *p) {
    free_lex_token((lex_token *) p);
}

// Lexer Helper Functions

/**
 * @brief Appends a single lex_token to the lex_token_list
 *
 * @param list token list being built.
 * @param token token being pushed.
 * @return non-zero if failed.
 */
static int token_push(lex_token_list *list, lex_token *token) {
    if (!list) return -1;
    if (list->cap == 0) {
        lex_token **temp = realloc(list->data, 4 * sizeof(lex_token *));
        if (!temp) {
            perror("realloc");
            return -1;
        }
        list->data = temp;
        list->cap = 4;
    } else if (list->len + 2 > list->cap) {
        lex_token **temp = realloc(list->data, 2 * list->cap * sizeof(lex_token *));
        if (!temp) {
            perror("realloc");
            return -1;
        }
        list->data = temp;
        list->cap <<= 1;
    }
    list->data[list->len] = token;
    list->data[list->len + 1] = NULL;
    ++list->len;
    return 0;
}

/**
 * appends a single character to token buffer
 *
 * @param buf token buffer being built
 * @param c character being pushed
 * @return non-zero if failed.
 */
static int buf_push(lex_token_buf *buf, char c) {
    if (!buf) return -1;
    if (buf->cap == 0) {
        char *temp = realloc(buf->data, 4 * sizeof(char));
        if (!temp) {
            perror("realloc");
            return -1;
        }
        buf->data = temp;
        buf->cap = 4;
    } else if (buf->len + 2 > buf->cap) {
        char *temp = realloc(buf->data, 2 * buf->cap * sizeof(char));
        if (!temp) {
            perror("realloc");
            return -1;
        }
        buf->data = temp;
        buf->cap <<= 1;
    }
    buf->data[buf->len] = c;
    buf->data[buf->len + 1] = 0x00;
    ++buf->len;
    return 0;
}

/**
 * @brief checks if the character is a valid whitespace.
 *
 * @param c character
 * @return 1 if whitespace, 0 otherwise
 */
static int is_whitespace(char c) {
    // Every character inside string (except NUL)
    for (size_t i = 0; i < sizeof(lex_whitespaces) / sizeof(lex_whitespaces[0]) - 1; ++i)
        if (c == lex_whitespaces[i]) return 1;
    return 0;
}

/**
 * @brief checks if the character is a valid operator.
 *
 * @param c character
 * @return 1 if operator, 0 otherwise
 */
static int is_operator(char c) {
    // Every character inside string (except NUL)
    for (size_t i = 0; i < sizeof(lex_operators) / sizeof(lex_operators[0]) - 1; ++i)
        if (c == lex_operators[i]) return 1;
    return 0;
}

/**
 * @brief Greedily expands operator pointed at *c, returns corresponding heap allocated token.
 * moves the passed pointer till the end of pointer.
 *
 * @param c string pointer address
 * @return Heap allocated corresponding token, NULL when error or operator not found.
 */
static lex_token *get_token(const char **c) {
    // Allocate operator token
    lex_token *tok = malloc(sizeof(lex_token));
    if (!tok) goto cleanup;

    // Emit operator token
    tok->data = NULL;
    switch (**c) {
        case ';':
            tok->type = TK_SEMICOLON;
            break;
        case '|':
            tok->type = TK_PIPE;
            if (*(*c + 1) == '|') {
                tok->type = TK_OR;
                ++(*c);
            }
            break;
        case '&':
            tok->type = TK_BG;
            if (*(*c + 1) == '&') {
                tok->type = TK_AND;
                ++(*c);
            }
            break;
        case '<':
            tok->type = TK_REDIR_IN;
            break;
        case '>':
            tok->type = TK_REDIR_OUT;
            if (*(*c + 1) == '>') {
                tok->type = TK_REDIR_APPEND;
                ++(*c);
            }
            break;
        default: goto cleanup;
    }

    tok->next_adj = !is_whitespace(*(*c + 1)) && *(*c + 1) != 0x00;

    return tok;

cleanup:
    free(tok);
    return NULL;
}

lex_token **lex_line(const char *str) {
    lex_token_list list = {
        .data = calloc(1, sizeof(lex_token *)),
        .cap = 1,
        .len = 0
    };

    lex_state esc_from = LEX_DEFAULT;
    lex_state state = LEX_DEFAULT;
    lex_token_buf buf = {
        .data = NULL,
        .cap = 0,
        .len = 0
    };
    lex_token *tok = NULL;

    if (!list.data) goto cleanup;

    for (const char *c = str; 1; ++c) {
        switch (state) {
            case LEX_DEFAULT:

                // State change
                if (*c == '\'') {
                    state = LEX_SINGLE_QUOTE;
                    break;
                }
                if (*c == '\"') {
                    state = LEX_DOUBLE_QUOTE;
                    break;
                }
                if (*c == '\\') {
                    esc_from = LEX_DEFAULT;
                    state = LEX_ESC;
                    break;
                }

                if (!is_operator(*c) && !is_whitespace(*c) && *c != 0x00) {
                    // Add character to token buffer
                    if (buf_push(&buf, *c)) goto cleanup;
                    break;
                }

                if (buf.len > 0) {
                    // Allocate token
                    tok = malloc(sizeof(lex_token));
                    if (!tok) goto cleanup;

                    // Emit token
                    tok->data = buf.data;
                    tok->next_adj = !is_whitespace(*c) && *c != 0x00;
                    tok->type = TK_DEFAULT;

                    // Reset buffer (before token_push) to avoid double free on error.
                    buf.data = NULL;
                    buf.cap = 0;
                    buf.len = 0;

                    if (token_push(&list, tok)) goto cleanup;

                    // Reset token and avoid double free
                    tok = NULL;
                }

                // Emit operator token
                if (is_operator(*c)) {
                    tok = get_token(&c);
                    if (!tok || token_push(&list, tok)) goto cleanup;

                    // Reset token and avoid double free
                    tok = NULL;
                }
                break;
            case LEX_SINGLE_QUOTE:
                if (*c == 0x00) {
                    fprintf(stderr, "Unterminated single quotation.");
                    goto cleanup;
                }
                if (*c == '\'') {
                    state = LEX_DEFAULT;
                    break;
                }
                if (buf_push(&buf, *c)) goto cleanup;
                break;

            case LEX_DOUBLE_QUOTE:
                if (*c == 0x00) {
                    fprintf(stderr, "Unterminated double quotation.");
                    goto cleanup;
                }
                if (*c == '\"') {
                    state = LEX_DEFAULT;
                    break;
                }
                if (*c == '\\') {
                    esc_from = LEX_DOUBLE_QUOTE;
                    state = LEX_ESC;
                    break;
                }
                if (buf_push(&buf, *c)) goto cleanup;
                break;
            case LEX_ESC:
                if (*c == 0x00) {
                    fprintf(stderr, "Unterminated escape character.");
                    goto cleanup;
                }
                switch (esc_from) {
                    case LEX_DEFAULT:
                        if (buf_push(&buf, *c)) goto cleanup;
                        break;
                    case LEX_DOUBLE_QUOTE:
                        if (*c == '\\' || *c == '\"') {
                            if (buf_push(&buf, *c)) goto cleanup;
                        } else {
                            if (buf_push(&buf, '\\')) goto cleanup;
                            if (buf_push(&buf, *c)) goto cleanup;
                        }
                        break;
                    default:
                        fprintf(stderr, "invalid esc_from.");
                        goto cleanup;
                }
                state = esc_from;
                break;
        }

        if (*c == 0x00) break;
    }

    return list.data;

cleanup:
    free_lex_token(tok);
    free(buf.data);
    free_ptrv((void **) list.data, free_lex_token_adapter);
    return NULL;
}


void print_token(lex_token *tok) {
    switch (tok->type) {
        case TK_DEFAULT:
            printf("DEFAULT(%s, adj=%d)", tok->data, tok->next_adj);
            break;
        case TK_SEMICOLON:
            printf("SEMICOLON(adj=%d)", tok->next_adj);
            break;
        case TK_PIPE:
            printf("PIPE(adj=%d)", tok->next_adj);
            break;
        case TK_BG:
            printf("BG(adj=%d)", tok->next_adj);
            break;
        case TK_REDIR_IN:
            printf("IN(adj=%d)", tok->next_adj);
            break;
        case TK_REDIR_OUT:
            printf("OUT(adj=%d)", tok->next_adj);
            break;
        case TK_REDIR_APPEND:
            printf("APPEND(adj=%d)", tok->next_adj);
            break;
        case TK_AND:
            printf("AND(adj=%d)", tok->next_adj);
            break;
        case TK_OR:
            printf("OR(adj=%d)", tok->next_adj);
            break;
        default:
            fprintf(stderr, "INVALID_TOKEN_TYPE");
            break;
    }
}
