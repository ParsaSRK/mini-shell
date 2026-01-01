#include "builtin.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "parse.h"

static builtin_cmd builtins[] = {
    {"exit", exit_fn},
    {NULL, NULL}
};

// TODO: i/o redir for builtins

int exit_fn(cmd_node *node, int *status) {
    if (!node || !node->argv || !node->argv[0]) return -1;
    if (node->argv[1] == NULL) exit(0);
    if (node->argv[2] != NULL) {
        fprintf(stderr, "exit: too many arguments!\n");
        return -1;
    }

    char *endptr;
    long long code = strtoll(node->argv[1], &endptr, 10);
    if (*endptr != 0x00) {
        fprintf(stderr, "exit: Numeric exit code required!\n");
        return 1; // user mistake, don't return -1 to not break NODE_SEQ functionality
    }

    exit((int) (code & 0xff));
}

int is_builtin(cmd_node *node) {
    if (!node || !node->argv || node->argv[0] == NULL)
        return 0;
    for (builtin_cmd *it = builtins; it->name != NULL; ++it) {
        if (strcmp(it->name, node->argv[0]) == 0) return 1;
    }
    return 0;
}

int run_builtin(cmd_node *node, int *status) {
    if (!node || !node->argv || node->argv[0] == NULL) {
        fprintf(stderr, "run_builtin: Invalid command!\n");
        return -1;
    }

    for (builtin_cmd *it = builtins; it->name != NULL; ++it) {
        if (strcmp(it->name, node->argv[0]) == 0) return it->fn(node, status);
    }

    fprintf(stderr, "run_builtin: builtin command not found!\n");
    return -1;
}
