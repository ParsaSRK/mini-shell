#include "builtin.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <linux/limits.h>

#include "parse.h"
#include "utils.h"

static builtin_cmd builtins[] = {
    {"exit", exit_fn},
    {"cd", cd_fn},
    {NULL, NULL}
};

int exit_fn(cmd_node *node, int *status) {
    if (!node || !node->argv || !node->argv[0]) return -1;

    if (node->argv[1] == NULL) exit(0);
    if (node->argv[2]) {
        fprintf(stderr, "exit: Too many arguments!\n");
        if (status)*status = 1;
        return 1; // user mistake
    }

    char *endptr;
    long long code = strtoll(node->argv[1], &endptr, 10);
    if (*endptr != 0x00) {
        fprintf(stderr, "exit: Numeric exit code required!\n");
        if (status)*status = 1;
        return 1; // user mistake
    }

    exit((int) (code & 0xff));
}

int cd_fn(cmd_node *node, int *status) {
    if (!node || !node->argv || !node->argv[0]) return -1;
    if (node->argv[1] && node->argv[2]) {
        fprintf(stderr, "cd: Too many arguments!\n");
        if (status) *status = 1;
        return 1; // user mistake
    }

    char *target;
    char oldpwd[PATH_MAX];
    if (!getcwd(oldpwd, sizeof(oldpwd))) {
        perror("cd: getcwd");
        if (status)*status = 1;
        return -1;
    }

    if (node->argv[1] == NULL) {
        target = getenv("HOME");
        if (!target) {
            fprintf(stderr, "cd: HOME not set!\n");
            if (status)*status = 1;
            return 1;
        }
    } else if (strcmp(node->argv[1], "-") == 0) {
        target = getenv("OLDPWD");
        if (!target) {
            fprintf(stderr, "cd: OLDPWD not set!\n");
            if (status)*status = 1;
            return 1;
        }
    } else {
        target = node->argv[1];
    }

    if (chdir(target)) {
        perror("cd: chdir");
        if (status)*status = 1;
        return 1;
    }

    setenv("OLDPWD", oldpwd, 1);
    char newpwd[PATH_MAX];
    if (getcwd(newpwd, sizeof(newpwd))) setenv("PWD", newpwd, 1);

    if (status)*status = 0;
    return 0;
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

    if (node->io && apply_redir(node) == -1)
        return -1;

    for (builtin_cmd *it = builtins; it->name != NULL; ++it) {
        if (strcmp(it->name, node->argv[0]) == 0) {
            int ret = it->fn(node, status);
            if (node->io) undo_redir();
            return ret;
        }
    }

    fprintf(stderr, "run_builtin: builtin command not found!\n");
    if (node->io) undo_redir();
    return -1;
}
