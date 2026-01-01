#pragma once
#include "parse.h"

typedef int (*builtin_fn)(cmd_node *node, int *status);

typedef struct builtin_cmd {
    const char *name;
    builtin_fn fn;
} builtin_cmd;

int exit_fn(cmd_node *node, int *status);

int is_builtin(cmd_node *node);

int run_builtin(cmd_node *node, int *status);
