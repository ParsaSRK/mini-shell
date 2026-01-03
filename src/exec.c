#include "exec.h"

#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

#include "parse.h"


#include "builtin.h"
#include "utils.h"

static void exec_child(cmd_node *cmd) {
    // Reset signals
    reset_signals();

    // Validate command
    if (!cmd || cmd->argv == NULL || cmd->argv[0] == NULL) {
        fprintf(stderr, "exec_child: Invalid command!\n");
        goto cleanup;
    }

    // Apply redirections
    if (cmd->io && apply_redir(cmd, REDIR_PERMANENTLY))
        goto cleanup;


    // Execute
    execvp(cmd->argv[0], cmd->argv);
    perror("execvp");
cleanup:
    _exit(127);
}


int execute_cmd(ast_node *node, int *status) {
    // Invalid node
    if (!node || node->type != NODE_CMD) {
        fprintf(stderr, "execute_cmd: Wrong node type!\n");
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
        exec_child(&node->as.cmd);
        _exit(127); // unreachable technically
    }

    // Parent process
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

int execute_pipe(ast_node *node, int *status) {
    int **pipes = NULL;
    pid_t *pids = NULL;

    if (!node || node->type != NODE_PIPE || !node->as.list.children) {
        fprintf(stderr, "execute_pipe: Wrong node type!\n");
        goto cleanup;
    }

    int cnt = 0;
    for (ast_node **it = node->as.list.children; *it != NULL; ++it)
        ++cnt;

    if (cnt < 2) {
        fprintf(stderr, "execute_pipe: children should be >= 2");
        goto cleanup;
    }

    pipes = calloc(cnt, sizeof(int *));
    if (!pipes) goto cleanup;

    pids = malloc(cnt * sizeof(pid_t));
    if (!pids) goto cleanup;
    for (int i = 0; i < cnt; ++i) pids[i] = -1;

    for (int i = 0; i < cnt - 1; ++i) {
        pipes[i] = calloc(2, sizeof(int));
        if (!pipes[i]) goto cleanup;
        if (pipe(pipes[i]) == -1) {
            perror("execute_pipe: pipe");
            goto cleanup;
        }
    }

    for (int i = 0; i < cnt; ++i) {
        ast_node *child = node->as.list.children[i];
        if (!child || child->type != NODE_CMD) {
            fprintf(stderr, "execute_pipe: Invalid child!\n");
            goto cleanup;
        }
        pids[i] = fork();
        if (pids[i] == -1) {
            perror("execute_pipe: fork");
            goto cleanup;
        }
        if (pids[i] != 0)
            continue;

        // child process
        if (
            (i > 0 && dup2(pipes[i - 1][0], STDIN_FILENO) == -1) ||
            (i < cnt - 1 && dup2(pipes[i][1], STDOUT_FILENO) == -1)
        ) {
            perror("execute_pipe: dup2");
            _exit(127);
        }

        for (int j = 0; j < cnt - 1; ++j) {
            close(pipes[j][0]);
            close(pipes[j][1]);
        }

        if (is_builtin(&child->as.cmd)) {
            int st = 0;
            reset_signals();
            run_builtin(&child->as.cmd, &st);
            _exit(st);
        }

        exec_child(&child->as.cmd);
        _exit(127);
    }

    for (int i = 0; i < cnt - 1; ++i) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    for (int i = 0; i < cnt; ++i) {
        int wstatus;
        if (waitpid(pids[i], &wstatus, 0) == -1) {
            perror("execute_pipe: waitpid");
            goto cleanup;
        }

        // set exit status code
        if (i == cnt - 1 && status) {
            if (WIFEXITED(wstatus)) *status = WEXITSTATUS(wstatus);
            else if (WIFSIGNALED(wstatus)) *status = 128 + WTERMSIG(wstatus);
            else *status = 1;
        }
    }

    free(pids);
    free_ptrv((void **) pipes, free);
    return 0;

cleanup:
    if (pipes) {
        for (int i = 0; i < cnt - 1; ++i) {
            if (pipes[i]) close(pipes[i][0]);
            if (pipes[i]) close(pipes[i][1]);
        }
        free_ptrv((void **) pipes, free);
    }

    if (pids) {
        for (int i = 0; i < cnt; ++i)
            if (pids[i] != -1) waitpid(pids[i], NULL, 0);
        free(pids);
    }
    if (status) *status = 1;
    return -1;
}

int execute_seq(ast_node *node, int *status) {
    if (!node || node->type != NODE_SEQ) {
        fprintf(stderr, "execute_seq: Wrong node type!\n");
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

int execute_and(ast_node *node, int *status) {
    if (!node || node->type != NODE_AND) {
        fprintf(stderr, "execute_and: Wrong node type!\n");
        return -1;
    }
    if (!node->as.binary.left || !node->as.binary.right) {
        fprintf(stderr, "execute_and: Wrong node data!\n");
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
        fprintf(stderr, "execute_or: Wrong node type!\n");
        return -1;
    }
    if (!node->as.binary.left || !node->as.binary.right) {
        fprintf(stderr, "execute_or: Wrong node data!\n");
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
        case NODE_CMD:
            return execute_cmd(node, status);
        case NODE_BG:
            return 0;
        case NODE_PIPE:
            return execute_pipe(node, status);
        case NODE_SEQ:
            return execute_seq(node, status);
        case NODE_AND:
            return execute_and(node, status);
        case NODE_OR:
            return execute_or(node, status);
        default:
            return -1;
    }
}
