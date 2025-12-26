#pragma once
#include <stddef.h>

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
    TK_SEMICOLON, ///< semicolon token ';'
    TK_PIPE, ///< pipe token '|'
    TK_BG, ///< background token '&'
    TK_REDIR_IN, ///< in redirection token '<'
    TK_REDIR_OUT, ///< out redirection token '>'
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

// API functions

/**
 * @brief Free a Lex Token.
 *
 * @param token Heap-allocated token.
 */
void free_lex_token(lex_token *token);

/**
 * @brief Adapter for free_lex_token to match void* destructor callbacks.
 *
 * Used when freeing generic pointer vectors.
 *
 * @param p Pointer to a lex_token.
 */
void free_lex_token_adapter(void *p);

/**
 * @brief Tokenizes the string to be parsed.
 *
 * @param str the string being tokenized
 * @return Heap-allocated, NULL-terminated lex_token list (or NULL on error)
 */
lex_token **lex_line(const char *str);

/**
 * @brief Debugging function to print token.
 * @param tok token being printed
 */
void print_token(lex_token *tok);