#pragma once

// Abstract Syntax Tree Structures

/**
 * @brief Node types for AST.
 */
typedef enum node_type {
    NODE_SEQ, // Sequence of commands, separated by ';'
    NODE_BG, // Background, terminated by '&'
    NODE_PIPE, // Pipe, separated by '|'
    NODE_CMD, // Command (leaves of the tree)
} node_type;

// declaration for recursive structure
typedef struct ast_node ast_node;

/**
 * @brief Used for NODE_PIPE and NODE_SEQ as
 * both have the same structure.
 *
 * @field children  Heap-allocated children node list
 */
typedef struct list_node {
    ast_node **children;
} list_node;

/**
 * @brief Used for NODE_BG
 *
 * @field child pointer to Heap-allocated child node
 */
typedef struct bg_node {
    ast_node *child;
} bg_node;

/**
 * @brief used for NODE_CMD
 *
 * @field argv Heap-allocated, NULL-terminated argument list.
 * @field in Heap-allocated, stdin file name (or NULL to inherit).
 * @field out Heap-allocated, stdout file name (or NULL to inherit).
 * @field err Heap-allocated, stderr file name (or NULL to inherit).
 */
typedef struct cmd_node {
    char **argv;
    char *in;
    char *out;
    char *err;
} cmd_node;

/**
 * @brief Abstract Syntax Tree Node.
 *
 * @field type Node type
 * @field as An abstraction to node information based on type
 */
typedef struct ast_node {
    node_type type;

    union {
        list_node list;
        bg_node bg;
        cmd_node cmd;
    } as;
} ast_node;


// API

/**
 * Free an AST Node.
 *
 * @param node Pointer to an ast_node.
 */
void free_ast_node(ast_node *node);

/**
 * @brief Parses a line of input to an AST
 *
 * @param str line of input
 * @return Heap-allocated lexed and parsed AST
 */
ast_node *parse_line(const char *str);
