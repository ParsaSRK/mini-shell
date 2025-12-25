#include "parse.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Lexer Structures

/**
 * @brief Lexing FSM used by the lexer.
 */
typedef enum lex_state {
    LEX_DEFAULT, ///< normal state
    LEX_DOUBLE_QUOTE, ///< inside double quotation "
    LEX_SINGLE_QUOTE, ///< inside single quotation '
    LEX_ESC, ///< escape character '\'
} lex_state;

/**
 * @brief Token type produced by the lexer
 */
typedef enum lex_token_type {
    TK_DEFAULT, ///< regular word token
    TK_PIPE, ///< pipe token '|'
    TK_REDIR_OUT, ///< out redirection token '>'
    TK_REDIR_IN, ///< in redirection token '<'
    TK_BG, ///< background token '&'
} lex_token_type;

/**
 * @brief Lexer token holding type and text.
 *
 */
typedef struct lex_token {
    lex_token_type type; ///< Token classification
    char *data; ///< Heap-allocated token text (or NULL for operators).
    int next_adj; ///< Nonzero when adjacent to the next token (no whitespace).
} lex_token;

/**
 * @brief Token buffer used while building the current word token.
 */
typedef struct lex_token_buf {
    char *data; ///< Heap-allocated, NUL-terminated character buffer.
    size_t len; ///< Current length in use (excluding NUL).
    size_t cap; ///< Allocated capacity of buf.
} lex_token_buf;

/**
 * @brief Dynamic list of lex_token pointers.
 * NULL-terminated when tokenization completes.
 */
typedef struct lex_token_list {
    lex_token **data; ///< Heap-allocated, NULL-terminated tokens list.
    size_t len; ///< Number of tokens stored (excluding the NULL terminator).
    size_t cap; ///< Allocated capacity of tokens.
} lex_token_list;

// Memory Management Functions

/**
 * Free a NULL-terminated array of pointers.
 *
 * @param arr     Heap-allocated, NULL-terminated pointer array.
 * @param destroy Destructor for one element, or NULL.
 */
static void free_ptrv(void **arr, void (*destroy)(void *)) {
    if (arr) {
        for (void **i = arr; *i != NULL; ++i)
            if (destroy) destroy(*i);
        free(arr);
    }
}

/**
 * @brief Free a Lex Token.
 *
 * @param token Heap-allocated token.
 */
static void free_lex_token(lex_token *token) {
    free(token->data);
    free(token);
}

/**
 * @brief Adapter for free_lex_token to match void* destructor callbacks.
 *
 * Used when freeing generic pointer vectors.
 *
 * @param p Pointer to a lex_token.
 */
static void free_lex_token_adapter(void *p) {
    free_lex_token((lex_token *) p);
}

/**
 * @brief Adapter for free_ast_node to match void* destructor callbacks.
 *
 * Used when freeing generic pointer vectors.
 *
 * @param p Pointer to an ast_node.
 */
static void free_ast_node_adapter(void *p) {
    free_ast_node((ast_node *) p);
}

void free_ast_node(ast_node *node) {
    if (!node) return;

    switch (node->type) {
        case NODE_BG:
            free_ast_node(node->as.bg.child);
            break;
        case NODE_SEQ:
        case NODE_PIPE:
            free_ptrv((void **) node->as.list.children, free_ast_node_adapter);
            break;
        case NODE_CMD:
            free_ptrv((void **) node->as.cmd.argv, free);
            free(node->as.cmd.in);
            free(node->as.cmd.out);
            free(node->as.cmd.err);
            break;
        default:
            break;
    }
    free(node);
}

// Lexer helper functions

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

static int is_special(char c) {
    switch (c) {
        case '\0':
        case ' ':
        case '\t':
        case '\n':
        case ';':
        case '|':
        case '&':
        case '<':
        case '>':
            return 1;
        default:
            return 0;
    }
}

// Lexer
/**
 * @brief Tokenizes the string to be parsed.
 *
 * @param str the string being tokenized
 * @return Heap-allocated, NULL-terminated lex_token list (or NULL on error)
 */
static lex_token **lex_line(const char *str) {
    lex_token_list list = {
        .data = NULL,
        .cap = 0,
        .len = 0
    };

    lex_state state = LEX_DEFAULT;
    lex_token_buf buf = {
        .data = NULL,
        .cap = 0,
        .len = 0
    };

    for (const char *cur = str; 1; ++cur) {
        switch (state) {
            case LEX_DEFAULT:
                if (!is_special(*cur)) {
                    // if (buf_push(&buf, *cur)) {
                    //     free(buf.data);
                    //     free_ptrv((void**)list.data, free_lex_token_adapter);
                    //     return NULL;
                    // }
                } else {
                    // TODO: emit token & if operator -> emit operator token
                }
                break;
            case LEX_SINGLE_QUOTE:
                // TODO: read until single quote
                break;
            case LEX_DOUBLE_QUOTE:
                // TODO: read until double quote
                break;
            case LEX_ESC:
                // TODO: escape next character
                break;
        }

        if (*cur == 0x00) break;
    }

    return list.data;
}

// Parsers
ast_node *parse_line(const char *line) {
    if (!line) return NULL;

    lex_token **tokens = lex_line(line);
    if (!tokens) return NULL;

    // TODO

    free_ptrv((void **) tokens, free_lex_token_adapter);

    return NULL; // temporary
}
