#pragma once

// Abstract Syntax Tree Structures

/**
 * @brief Node types for AST.
 */
typedef enum node_type {
    NODE_SEQ, ///< Sequence of commands, separated by ';'
    NODE_BG, ///< Background, terminated by '&'
    NODE_PIPE, ///< Pipe, separated by '|'
    NODE_CMD, ///< Command (leaves of the tree)
    NODE_AND, ///< AND operator, separated by '&&'
    NODE_OR, ///< OR operator, separated by '||'
} node_type;

typedef struct ast_node ast_node; // declaration for recursive structure

/**
 * @brief used for NODE_AND, and NODE_OR
 * as both have the same structure
 */
typedef struct binary_node {
    ast_node *left; ///< left part
    ast_node *right; ///< right part
} binary_node;

/**
 * @brief Used for NODE_PIPE and NODE_SEQ as
 * both have the same structure.
 */
typedef struct list_node {
    ast_node **children; ///< Heap-allocated children node list
} list_node;

/**
 * @brief Used for NODE_BG
 */
typedef struct bg_node {
    ast_node *child; ///< pointer to Heap-allocated child node
} bg_node;

typedef enum redir_type {
    REDIR_IN, ///<
    REDIR_OUT,
    REDIR_APPEND
} redir_type;

typedef struct redir {
    int fd;
    redir_type type;
    char *filename;
} redir;

/**
 * @brief used for NODE_CMD
 */
typedef struct cmd_node {
    char **argv; ///< Heap-allocated, NULL-terminated argument list.
    redir **io;
} cmd_node;

/**
 * @brief Abstract Syntax Tree Node.
 */
typedef struct ast_node {
    node_type type; ///< Node type

    union {
        binary_node binary;
        list_node list;
        bg_node bg;
        cmd_node cmd;
    } as; ///< An abstraction to node information based on type
} ast_node;


/**
 * @brief Free a redir struct
 *
 * @param io Pointer to a redir struct
 */
void free_redir(redir *io);

/**
 * @brief Adapter for free_redir to match void* destructor callbacks.
 * Used when freeing generic pointer vectors.
 *
 * @param p Pointer to a redir struct
 */
void free_redir_adapter(void *p);

/**
 * @brief Free an AST Node.
 *
 * @param node Pointer to an ast_node.
 */
void free_ast_node(ast_node *node);

/**
 * @brief Adapter for free_ast_node to match void* destructor callbacks.
 * Used when freeing generic pointer vectors.
 *
 * @param p Pointer to an ast_node.
 */
void free_ast_node_adapter(void *p);


/**
 * @brief recursively prints nodes of a valid AST tree.
 *
 * @param root Root node of the AST tree
 * @param depth current depth of the node (default = 0, used for indentation)
 */
void print_ast(const ast_node *root, int depth);

/**
 * @brief Parses a line of input to an AST
 *
 * @param str line of input
 * @return Heap-allocated lexed and parsed AST
 */
ast_node *parse_line(const char *str);
