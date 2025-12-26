#include "parse.h"
#include "lex.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Memory Management Functions

void free_ast_node_adapter(void *p) {
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

// Parser

ast_node *parse_line(const char *line) {
    if (!line) return NULL;

    lex_token **tokens = lex_line(line);
    if (!tokens) return NULL;

    for (lex_token **tok = tokens; *tok != NULL; ++tok) {
        print_token(*tok);
        printf(" ");
    }
    printf("\n");

    free_ptrv((void **) tokens, free_lex_token_adapter);

    return NULL; // temporary
}
