#include "exec.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>

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

    pid_t pid = -1;
    job *j = NULL;

    pid = fork();
    switch (pid) {
        case -1: // Error
            perror("fork");
            return -1;

        case 0: // Child Process
            if (setpgid(0, 0) == -1 && errno != EACCES && errno != EINTR) {
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
        kill(pid, SIGKILL);
        waitpid(pid, NULL, 0);
        return -1;
    }

    // Allocate a job
    j = calloc(1, sizeof(job));
    if (!j) {
        perror("execute_cmd: calloc");
        kill(pid, SIGKILL);
        waitpid(pid, NULL, 0);
        return -1;
    }
    j->id = -1;
    j->procs = calloc(1, sizeof(process));
    if (!j->procs) {
        perror("execute_cmd: calloc");
        kill(pid, SIGKILL);
        waitpid(pid, NULL, 0);
        free_job(j);
        return -1;
    }

    // Build job's process description
    j->nproc = 1;
    j->procs[0].state = PROC_RUN;
    j->procs[0].pid = pid;
    j->procs[0].exit_code = -1;
    j->procs[0].term_sig = -1;

    // Build job description
    j->id = getId();
    if (j->id == -1) {
        fprintf(stderr, "execute_cmd: Job table full!\n");
        kill(pid, SIGKILL);
        waitpid(pid, NULL, 0);
        free_job(j);
        return -1;
    }
    j->isbg = isbg;
    j->pgid = pid;
    j->isupd = 0;
    j->next = NULL;
    j->state = JOB_RUNNING;

    add_job(j);

    // dont want for finish if bg
    if (isbg) {
        if (status) *status = 0;
        return 0;
    }

    // Pass the terminal
    if (isatty(STDIN_FILENO) && tcsetpgrp(STDIN_FILENO, pid) == -1)
        perror("execute_cmd: tcsetpgrp");

    // Wait for child
    int wstatus;
    while (waitpid(pid, &wstatus, WUNTRACED) == -1) {
        if (errno == EINTR) continue;
        assert(!"execute_cmd: waitpid failed unexpectedly");
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
    job *j = NULL;

    if (!node || node->type != NODE_PIPE || !node->as.list.children) {
        fprintf(stderr, "execute_pipe: Wrong node type!\n");
        goto cleanup;
    }

    int cnt = 0;
    for (ast_node **it = node->as.list.children; *it != NULL; ++it)
        ++cnt;

    if (cnt < 2) {
        fprintf(stderr, "execute_pipe: Children count should be >= 2");
        goto cleanup;
    }

    pipes = calloc(cnt, sizeof(int *));
    if (!pipes) goto cleanup;

    j = calloc(1, sizeof(job));
    if (!j) {
        perror("execute_pipe: calloc");
        goto cleanup;
    }
    j->id = getId();
    if (j->id == -1) {
        fprintf(stderr, "execute_pipe: Job table full!\n");
        goto cleanup;
    }

    j->procs = calloc(cnt, sizeof(process));
    if (!j->procs) {
        perror("execute_pipe: calloc");
        goto cleanup;
    }
    j->nproc = cnt;
    j->isbg = isbg;
    j->isupd = 0;
    j->pgid = 0;
    j->state = JOB_RUNNING;

    for (int i = 0; i < cnt; ++i) {
        j->procs[i].pid = -1;
        j->procs[i].exit_code = -1;
        j->procs[i].term_sig = -1;
        j->procs[i].state = PROC_RUN;
    }

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
        j->procs[i].pid = fork();
        if (j->procs[i].pid == -1) {
            perror("execute_pipe: fork");
            goto cleanup;
        }
        if (j->procs[i].pid != 0) {
            if (i == 0) {
                j->pgid = j->procs[0].pid;
                if (setpgid(j->procs[0].pid, j->pgid) == -1 && errno != EACCES && errno != EINTR) {
                    perror("execute_pipe: setpgid");
                    goto cleanup;
                }
                if (!isbg && isatty(STDIN_FILENO) && tcsetpgrp(STDIN_FILENO, j->pgid) == -1)
                    perror("execute_pipe: tcsetpgrp");
            } else {
                if (setpgid(j->procs[i].pid, j->pgid) == -1 && errno != EACCES && errno != EINTR) {
                    perror("execute_pipe: setpgid");
                    goto cleanup;
                }
            }
            continue;
        }

        // child process
        if (setpgid(0, j->pgid) == -1 && errno != EACCES && errno != EINTR) {
            perror("execute_pipe: setpgid");
            _exit(127);
        }

        if (
            (i > 0 && dup2(pipes[i - 1][0], STDIN_FILENO) == -1) ||
            (i < cnt - 1 && dup2(pipes[i][1], STDOUT_FILENO) == -1)
        ) {
            perror("execute_pipe: dup2");
            _exit(127);
        }

        for (int k = 0; k < cnt - 1; ++k) {
            close(pipes[k][0]);
            close(pipes[k][1]);
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

    add_job(j);
    if (isbg) {
        free_ptrv((void **) pipes, free);
        if (status) *status = 0;
        return 0;
    }

    while (1) {
        pid_t pid = 0;
        int wstat;
        pid = waitpid(-j->pgid, &wstat, WUNTRACED);
        if (pid > 0) {
            update_proc(pid, wstat);
            update_job(j);
            if (j->state != JOB_RUNNING) break;
            continue;
        }
        if (pid == -1) {
            if (errno == EINTR) continue;
            if (errno == ECHILD) break;
            perror("fg: waitpid");
            break;
        }
    }

    // Set status
    process *last_proc = j->procs + j->nproc - 1;
    if (status) {
        if (last_proc->exit_code != -1) *status = last_proc->exit_code;
        else if (last_proc->term_sig != -1) *status = 128 + last_proc->term_sig;
    }

    // Reclaim the terminal
    if (isatty(STDIN_FILENO) && tcsetpgrp(STDIN_FILENO, getpgrp()) == -1)
        perror("execute_cmd: tcsetpgrp");

    free_ptrv((void **) pipes, free);
    return 0;

cleanup:
    // Reclaim the terminal
    if (isatty(STDIN_FILENO) && tcsetpgrp(STDIN_FILENO, getpgrp()) == -1)
        perror("execute_cmd: tcsetpgrp");

    if (pipes) {
        for (int i = 0; i < cnt - 1; ++i) {
            if (pipes[i]) close(pipes[i][0]);
            if (pipes[i]) close(pipes[i][1]);
        }
        free_ptrv((void **) pipes, free);
    }

    if (j && j->procs) {
        for (int i = 0; i < j->nproc; ++i)
            if (j->procs[i].pid != -1) {
                kill(j->procs[i].pid, SIGKILL);
                waitpid(j->procs[i].pid, NULL, 0);
            }
    }
    if (j) free_job(j);

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
