#include "parse.h"

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
    char *buf; ///< Heap-allocated character buffer.
    size_t len; ///< Current length in use (excluding NUL).
    size_t cap; ///< Allocated capacity of buf.
} lex_token_buf;

/**
 * @brief Dynamic list of lex_token pointers.
 * NULL-terminated when tokenization completes.
 */
typedef struct lex_token_list {
    lex_token **tokens; ///< Heap-allocated tokens list.
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
 * Free a Lex Token.
 *
 * @param token Heap-allocated token.
 */
static void free_lex_token(lex_token *token) {
    free(token->data);
    free(token);
}

/**
 * Adapter for free_lex_token to match void* destructor callbacks.
 *
 * Used when freeing generic pointer vectors.
 *
 * @param p Pointer to a lex_token.
 */
static void free_lex_token_adapter(void *p) {
    free_lex_token((lex_token *) p);
}

/**
 * Adapter for free_ast_node to match void* destructor callbacks.
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
 * @param tokens token list being built.
 * @param token token being pushed.
 * @return non-zero if failed.
 */
static int push_token(lex_token_list *tokens, lex_token *token) {
    if (!tokens || !token) return -1;

    return 0;
}

// Lexer
/**
 * @brief Tokenizes the string to be parsed.
 *
 * @param str the string being tokenized
 * @return Heap-allocated, NULL-terminated lex_token list (or NULL on error)
 */
static lex_token **lex_line(const char *str) {
    // TODO

    return NULL; // temporary
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
