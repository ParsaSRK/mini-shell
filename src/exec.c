#include "exec.h"

#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdio.h>

#include "parse.h"


#include "builtin.h"
#include "utils.h"

int execute_seq(ast_node *node, int *status) {
    if (!node || node->type != NODE_SEQ) {
        fprintf(stderr, "execute_seq: wrong node type!\n");
        return -1;
    }

    if (!node->as.list.children || node->as.list.children[0] == NULL) {
        if (status) *status = 0;
        return 0;
    }

    for (ast_node **it = node->as.list.children; *it != NULL; ++it) {
        int ret = execute_ast(*it, status);
        if (ret != 0) return ret;
    }
    return 0;
}

int execute_cmd(ast_node *node, int *status) {
    // Invalid node
    if (!node || node->type != NODE_CMD) {
        fprintf(stderr, "execute_cmd: wrong node type!\n");
        return -1;
    }

    // Empty command
    if (node->as.cmd.argv == NULL
        || node->as.cmd.argv[0] == NULL) {
        if (status) *status = 0;
        return 0;
    }

    // Run if builtin function
    if (is_builtin(&node->as.cmd))
        return run_builtin(&node->as.cmd, status);

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return -1;
    }

    // Child process
    if (pid == 0) {

        // Setup redirections
        if (node->as.cmd.io && apply_redir(&node->as.cmd) == -1)
            _exit(127);

        // Change process image
        execvp(node->as.cmd.argv[0], node->as.cmd.argv);
        perror("execvp");
        _exit(127);
    }

    // parent process
    int wstatus;
    if (waitpid(pid, &wstatus, 0) == -1) {
        perror("waitpid");
        return -1;
    }

    // set exit status code
    if (status) {
        if (WIFEXITED(wstatus)) *status = WEXITSTATUS(wstatus);
        else if (WIFSIGNALED(wstatus)) *status = 128 + WTERMSIG(wstatus);
        else *status = 1;
    }
    return 0;
}


int execute_and(ast_node *node, int *status) {
    if (!node || node->type != NODE_AND) {
        fprintf(stderr, "execute_and: wrong node type!\n");
        return -1;
    }
    if (!node->as.binary.left || !node->as.binary.right) {
        fprintf(stderr, "execute_and: wrong node data!\n");
        return -1;
    }

    int wstatus = 0;
    int ret = execute_ast(node->as.binary.left, &wstatus);
    if (ret != 0)
        return ret;

    if (wstatus != 0) {
        if (status) *status = wstatus;
        return 0;
    }

    return execute_ast(node->as.binary.right, status);
}

int execute_or(ast_node *node, int *status) {
    if (!node || node->type != NODE_OR) {
        fprintf(stderr, "execute_or: wrong node type!\n");
        return -1;
    }
    if (!node->as.binary.left || !node->as.binary.right) {
        fprintf(stderr, "execute_or: wrong node data!\n");
        return -1;
    }

    int wstatus = 0;
    int ret = execute_ast(node->as.binary.left, &wstatus);
    if (ret != 0)
        return ret;

    if (wstatus == 0) {
        if (status) *status = wstatus;
        return 0;
    }

    return execute_ast(node->as.binary.right, status);
}

int execute_ast(ast_node *node, int *status) {
    if (!node) return -1;
    switch (node->type) {
        case NODE_SEQ:
            return execute_seq(node, status);
        case NODE_BG:
            return 0;
        case NODE_PIPE:
            return 0;
        case NODE_CMD:
            return execute_cmd(node, status);
        case NODE_AND:
            return execute_and(node, status);
        case NODE_OR:
            return execute_or(node, status);
        default:
            return -1;
    }
}
