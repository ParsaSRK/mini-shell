#include "parse.h"

#include <errno.h>
#include <limits.h>

#include "lex.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Memory Management Functions

void free_redir(redir *io) {
    free(io->path);
    free(io);
}

void free_redir_adapter(void *p) {
    free_redir((redir *) p);
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
            free_ptrv((void **) node->as.cmd.io, free_redir_adapter);
            break;
        case NODE_AND:
        case NODE_OR:
            free_ast_node(node->as.binary.left);
            free_ast_node(node->as.binary.right);
            break;
        default:
            fprintf(stderr, "free_ast_node: Invalid node type!\n");
    }
    free(node);
}

void free_ast_node_adapter(void *p) {
    free_ast_node((ast_node *) p);
}

void print_ast(const ast_node *root, int depth) {
    if (!root) return;

    for (int i = 0; i < depth; ++i) putchar('-');
    putchar(' ');

    switch (root->type) {
        case NODE_SEQ:
            printf("NODE_SEQ\n");
            for (ast_node **it = root->as.list.children; *it != NULL; ++it)
                print_ast(*it, depth + 2);
            break;

        case NODE_PIPE:
            printf("NODE_PIPE\n");
            for (ast_node **it = root->as.list.children; *it != NULL; ++it)
                print_ast(*it, depth + 2);
            break;

        case NODE_AND:
            printf("NODE_AND\n");
            print_ast(root->as.binary.left, depth + 2);
            print_ast(root->as.binary.right, depth + 2);
            break;

        case NODE_OR:
            printf("NODE_OR\n");
            print_ast(root->as.binary.left, depth + 2);
            print_ast(root->as.binary.right, depth + 2);
            break;

        case NODE_BG:
            printf("BACKGROUND\n");
            print_ast(root->as.bg.child, depth + 2);
            break;

        case NODE_CMD:
            printf("[ ");
            for (char **it = root->as.cmd.argv; *it != NULL; ++it)
                printf("\"%s\" ", *it);
            printf("] ");
            if (*root->as.cmd.io) printf("I/O: ");
            for (redir **it = root->as.cmd.io; *it != NULL; ++it) {
                printf("%d%s%s ",
                       (*it)->fd,
                       (*it)->type == REDIR_IN ? "<" : (*it)->type == REDIR_OUT ? ">" : ">>",
                       (*it)->path);
            }
            printf("\n");
            break;

        default:
            fprintf(stderr, "print_ast: Invalid node type!\n");
    }
}

// Parser Helper functions

static int parse_fd(const char *data, int *out) {
    char *end = NULL;
    long v = 0;
    if (!data || *data == 0x00) return -1;

    errno = 0;
    v = strtol(data, &end, 10);
    if (errno != 0 || *end != 0x00 || v < 0 || v > INT_MAX) return -1;
    *out = (int) v;

    return 0;
}

static ast_node *parse_cmd(lex_token **l, lex_token **r) {
    ast_node *leaf = NULL;
    int *consumed = NULL;

    if (l == r) {
        fprintf(stderr, "parse_cmd: Empty segment not allowed!\n");
        goto cleanup;
    }

    leaf = calloc(1, sizeof(ast_node));
    if (!leaf) {
        perror("parse_cmd: calloc");
        goto cleanup;
    }
    leaf->type = NODE_CMD;

    int iocnt = 0;
    for (lex_token **it = l; it != r; ++it) {
        iocnt += (*it)->type == TK_REDIR_IN ||
                (*it)->type == TK_REDIR_OUT ||
                (*it)->type == TK_REDIR_APPEND;
    }

    leaf->as.cmd.io = calloc(iocnt + 1, sizeof(redir *));
    if (!leaf->as.cmd.io) {
        perror("parse_cmd: calloc");
        goto cleanup;
    }
    int i = 0;

    consumed = calloc(r - l, sizeof(int));
    if (!consumed) {
        perror("parse_cmd: calloc");
        goto cleanup;
    }

    for (lex_token **it = l; it != r; ++it) {
        if ((*it)->type != TK_REDIR_IN &&
            (*it)->type != TK_REDIR_OUT &&
            (*it)->type != TK_REDIR_APPEND)
            continue;

        // Allocate memory
        redir *io = calloc(1, sizeof(redir));
        if (!io) {
            perror("parse_cmd: calloc");
            goto cleanup;
        }

        leaf->as.cmd.io[i++] = io;

        // Set type and default fd
        if ((*it)->type == TK_REDIR_IN) {
            io->type = REDIR_IN;
            io->fd = 0;
        } else if ((*it)->type == TK_REDIR_OUT) {
            io->type = REDIR_OUT;
            io->fd = 1;
        } else {
            io->type = REDIR_APPEND;
            io->fd = 1;
        }
        consumed[it - l] = 1; // mark token as consumed.

        // Check and set the file name
        if (*(it + 1) == NULL || (*(it + 1))->type != TK_DEFAULT || (*(it + 1))->data == NULL) {
            fprintf(stderr, "parse_cmd: Invalid filename!\n");
            goto cleanup;
        }
        io->path = strdup((*(it + 1))->data);
        if (!io->path) {
            perror("parse_cmd: strdup");
            goto cleanup;
        }
        consumed[it - l + 1] = 1; // mark filename as consumed

        // Check and set fd
        int fd = 0;
        if (
            l < it && // not for first token ( no valid token before )
            (*(it - 1))->type == TK_DEFAULT && // should be default token (text)
            (*(it - 1))->next_adj && // should be adjacent
            !parse_fd((*(it - 1))->data, &fd) // valid fd
        ) {
            io->fd = fd;
            consumed[it - l - 1] = 1; // mark fd as consumed
        }
    }

    int argc = 0;
    for (lex_token **it = l; it != r; ++it)
        argc += !consumed[it - l];

    leaf->as.cmd.argv = calloc(argc + 1, sizeof(char *));
    if (!leaf->as.cmd.argv) {
        perror("parse_cmd: calloc");
        goto cleanup;
    }

    int idx = 0;
    for (lex_token **it = l; it != r; ++it) {
        if (!consumed[it - l]) {
            if ((*it)->type != TK_DEFAULT || (*it)->data == NULL) {
                fprintf(stderr, "parse_cmd: Invalid argv token!\n");
                goto cleanup;
            }
            leaf->as.cmd.argv[idx++] = strdup((*it)->data);
            if (!leaf->as.cmd.argv[idx - 1]) {
                perror("parse_cmd: strdup");
                goto cleanup;
            }
        }
    }

    free(consumed);
    return leaf;

cleanup:
    free(consumed);
    free_ast_node(leaf);
    return NULL;
}

static ast_node *parse_pipe(lex_token **l, lex_token **r) {
    ast_node *root = NULL;

    if (l == r) {
        fprintf(stderr, "parse_pipe: Empty segment not allowed!\n");
        goto cleanup;
    }

    int cnt = 0;
    for (lex_token **it = l; it != r; ++it)
        cnt += (*it)->type == TK_PIPE;

    // not a pipe
    if (!cnt)
        return parse_cmd(l, r);

    root = malloc(sizeof(ast_node));
    if (!root) {
        perror("parse_pipe: malloc");
        goto cleanup;
    }
    root->type = NODE_PIPE;
    root->as.list.children = NULL; // safe cleanup

    // cnt+1 segment + 1 NULL terminator
    root->as.list.children = calloc(cnt + 2, sizeof(ast_node *));
    if (!root->as.list.children) {
        perror("parse_pipe: calloc");
        goto cleanup;
    }

    int i = 0;
    lex_token **ll = l;
    for (lex_token **rr = l; 1; ++rr) {
        if (rr != r && (*rr)->type != TK_PIPE) continue;

        root->as.list.children[i] = parse_cmd(ll, rr);
        if (!root->as.list.children[i]) goto cleanup;
        ll = rr + 1;
        ++i;
        if (rr == r) break;
    }

    return root;
cleanup:
    free_ast_node(root);
    return NULL;
}

static int parse_and_or(lex_token **l, lex_token **r, ast_node **result) {
    // Predefine with NULL pointers to have safe cleanup
    *result = NULL;
    ast_node *head = NULL;

    if (l == r) return 1; // Empty segment

    // Check if it is and_or node
    // if it is, parse first segment (perhaps pipe)
    int is_and_or = 0;
    lex_token **it;
    for (it = l; it != r; ++it) {
        if ((*it)->type == TK_AND || (*it)->type == TK_OR) {
            is_and_or = 1;
            head = parse_pipe(l, it);
            if (!head) goto cleanup;
            break;
        }
    }

    // Not an and_or node
    if (!is_and_or) {
        *result = parse_pipe(l, r);
        if (!(*result)) return -1;
        return 0;
    }

    // Continue to parse the rest and store with left-associativity.
    ++it;
    lex_token **ll = it;
    for (; ; ++it) {
        if (it != r && (*it)->type != TK_AND && (*it)->type != TK_OR) continue;

        ast_node *new_head = calloc(1, sizeof(ast_node));
        if (!new_head) {
            perror("parse_and_or: calloc");
            goto cleanup;
        }

        // Immediately switching to head.
        new_head->as.binary.left = head;
        head = new_head;

        // This is the new head now
        head->type = (*(ll - 1))->type == TK_AND ? NODE_AND : NODE_OR;
        head->as.binary.right = parse_pipe(ll, it);
        if (!head->as.binary.right) goto cleanup;

        ll = it + 1;
        if (it == r) break;
    }

    *result = head;

    return 0;

cleanup:
    free_ast_node(head);
    return -1;
}

// Parser

ast_node *parse_line(const char *line) {
    if (!line) return NULL;

    // predefine to avoid ambiguity on cleanup
    ast_node *root = NULL;
    ast_node *child = NULL;

    // Tokenization
    lex_token **tokens = lex_line(line);
    if (!tokens) goto cleanup;

    int mxcnt = 1;
    for (lex_token **it = tokens; *it != NULL; ++it) {
        mxcnt += (*it)->type == TK_SEMICOLON || (*it)->type == TK_BG;
    }

    // Allocate the root ( NODE_SEQ )
    root = malloc(sizeof(ast_node));
    if (!root) {
        perror("parse_line: malloc");
        goto cleanup;
    }

    root->type = NODE_SEQ;
    root->as.list.children = calloc(mxcnt + 1, sizeof(ast_node *));
    if (!root->as.list.children) {
        perror("parse_line: calloc");
        goto cleanup;
    }

    int i = 0;
    lex_token **l = tokens;
    for (lex_token **r = tokens; 1; ++r) {
        if (*r != NULL && (*r)->type != TK_SEMICOLON && (*r)->type != TK_BG) continue;

        int res = parse_and_or(l, r, &child);
        if (res == -1) goto cleanup;
        if (res == 1) {
            if (*r == NULL) break;
            fprintf(stderr, "parse_line: Empty segment not allowed!\n");
            goto cleanup;
        }

        if (*r != NULL && (*r)->type == TK_BG) {
            ast_node *bg = malloc(sizeof(ast_node));
            if (!bg) {
                perror("parse_line: malloc");
                goto cleanup;
            }
            bg->type = NODE_BG;
            bg->as.bg.child = child;
            root->as.list.children[i++] = bg;
        } else {
            root->as.list.children[i++] = child;
        }

        child = NULL; // avoid double free on error

        l = r + 1;

        if (*r == NULL) break;
    }


    free_ptrv((void **) tokens, free_lex_token_adapter);

    return root;

cleanup:
    free_ptrv((void **) tokens, free_lex_token_adapter);
    free_ast_node(child);
    free_ast_node(root);
    return NULL;
}
