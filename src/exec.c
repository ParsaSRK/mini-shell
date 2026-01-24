#include "exec.h"

#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <asm-generic/errno-base.h>

#include "parse.h"
#include "redir.h"
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

int execute_cmd(ast_node *node, int *status, int isbg) {
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

    int jid = getId();
    if (jid == -1) {
        fprintf(stderr, "execute_cmd: Job table full!\n");
        return -1;
    }

    pid_t pid = fork();
    switch (pid) {
        case -1: // Error
            perror("fork");
            return -1;

        case 0: // Child Process
            if (setpgid(0, 0) == -1) {
                perror("execute_cmd: setpgid");
                _exit(127);
            }
            exec_child(&node->as.cmd);
            _exit(127); // unreachable technically

        default:
            break;
    }

    // Parent process

    // Set process group ID
    if (setpgid(pid, pid) == -1 && errno != EACCES && errno != EINTR) {
        perror("execute_cmd: setpgid");
        return -1;
    }

    // Allocate a job
    job *j = malloc(sizeof(job));
    if (!j) {
        perror("execute_cmd: malloc");
        return -1;
    }
    j->procs = malloc(sizeof(process));
    if (!j->procs) {
        perror("execute_cmd: malloc");
        free(j);
        return -1;
    }

    // Build job's process description
    j->nproc = 1;
    j->procs[0].state = PROC_RUN;
    j->procs[0].pid = pid;
    j->procs[0].exit_code = -1;
    j->procs[0].term_sig = -1;

    // Build job description
    j->id = jid;
    j->isbg = isbg;
    j->pgid = pid;
    j->isupd = 0;
    j->next = NULL;
    j->state = JOB_RUNNING;

    add_job(j);

    // dont want for finish if bg
    if (isbg)
        return 0;

    // Pass the terminal
    if (isatty(STDIN_FILENO) && tcsetpgrp(STDIN_FILENO, pid) == -1)
        perror("execute_cmd: tcsetpgrp");

    // Wait for child
    int wstatus;
    if (waitpid(pid, &wstatus, WUNTRACED) == -1) {
        perror("execute_cmd: waitpid");
        // Reclaim terminal before returning
        if (isatty(STDIN_FILENO) && tcsetpgrp(STDIN_FILENO, getpgrp()) == -1) perror("execute_cmd: tcsetpgrp");
        return -1; // No cleanup, ownership is for jobs.c
    }

    // Reclaim the terminal
    if (isatty(STDIN_FILENO) && tcsetpgrp(STDIN_FILENO, getpgrp()) == -1)
        perror("execute_cmd: tcsetpgrp");

    // Update jobs and processes
    if (update_proc(pid, wstatus) == -1) return -1; // No cleanup, ownership is for job.c

    // set exit status code
    if (status) {
        if (j->procs[0].exit_code != -1)
            *status = j->procs[0].exit_code;
        else if (j->procs[0].term_sig != -1)
            *status = 128 + j->procs[0].term_sig;
    }

    return 0;
}

int execute_pipe(ast_node *node, int *status, int isbg) {
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
        int ret = execute_ast(*it, status, 0);
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
    int ret = execute_ast(node->as.binary.left, &wstatus, 0);
    if (ret != 0)
        return ret;

    if (wstatus != 0) {
        if (status) *status = wstatus;
        return 0;
    }

    return execute_ast(node->as.binary.right, status, 0);
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
    int ret = execute_ast(node->as.binary.left, &wstatus, 0);
    if (ret != 0)
        return ret;

    if (wstatus == 0) {
        if (status) *status = wstatus;
        return 0;
    }

    return execute_ast(node->as.binary.right, status, 0);
}

int execute_bg(ast_node *node, int *status) {
    if (!node || node->type != NODE_BG) {
        fprintf(stderr, "execute_bg: Wrong node type!\n");
        return -1;
    }

    if (!node->as.bg.child) {
        fprintf(stderr, "execute_bg: Wrong node data!\n");
        return -1;
    }

    if (node->as.bg.child->type != NODE_PIPE && node->as.bg.child->type != NODE_CMD) {
        fprintf(stderr, "execute_bg: Only regular commands and pipes are allowed as background operation!\n");
        if (status) *status = 1;
        return 1;
    }

    return execute_ast(node->as.bg.child, status, 1);
}

int execute_ast(ast_node *node, int *status, int isbg) {
    if (!node) return -1;
    switch (node->type) {
        case NODE_CMD:
            return execute_cmd(node, status, isbg);
        case NODE_BG:
            return execute_bg(node, status);
        case NODE_PIPE:
            return execute_pipe(node, status, isbg);
        case NODE_SEQ:
            return execute_seq(node, status);
        case NODE_AND:
            return execute_and(node, status);
        case NODE_OR:
            return execute_or(node, status);
        default:
            fprintf(stderr, "execute_ast: Wrong node type!\n");
            if (status) *status = 1;
            return -1;
    }
}
