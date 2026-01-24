#include "builtin.h"

#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <asm-generic/errno-base.h>
#include <linux/limits.h>
#include <sys/wait.h>

#include "parse.h"
#include "redir.h"
#include "job.h"

/**
 * @brief builtin commands list terminated by {NULL, NULL}
 */
static builtin_cmd builtins[] = {
    {"exit", exit_fn},
    {"cd", cd_fn},
    {"jobs", jobs_fn},
    {"fg", fg_fn},
    {"bg", bg_fn},
    {NULL, NULL}
};

int bg_fn(cmd_node *node, int *status) {
    if (!node || !node->argv || !node->argv[0]) return -1;

    int id = 0;
    if (node->argv[1] == NULL) id = -1;
    else if (node->argv[1][0] != '%') {
        fprintf(stderr, "bg: Invalid Syntax! Usage: \"bg %%N\"\n");
        if (status) *status = 1;
        return 1;
    } else {
        char *endptr = NULL;
        id = (int) strtoll(node->argv[1] + 1, &endptr, 10);
        if (*endptr != 0x00) {
            fprintf(stderr, "bg: Numeric job ID required!\n");
            if (status) *status = 1;
            return 1;
        }
    }

    job *j = get_job(id);
    if (!j) {
        fprintf(stderr, "bg: Job not found!\n");
        if (status) *status = 1;
        return 1;
    }

    j->isbg = 1;
    kill(-j->pgid, SIGCONT);

    j->state = JOB_RUNNING;
    for (int i = 0; i < j->nproc; ++i) {
        if (j->procs[i].state != PROC_DONE) {
            j->procs[i].state = PROC_RUN;
        }
    }

    if (status) *status = 0;
    return 0;
}

int fg_fn(cmd_node *node, int *status) {
    if (!node || !node->argv || !node->argv[0]) return -1;

    int id = 0;
    if (node->argv[1] == NULL) id = -1;
    else if (node->argv[1][0] != '%') {
        fprintf(stderr, "fg: Invalid Syntax! Usage: \"fg %%N\"\n");
        if (status) *status = 1;
        return 1;
    } else {
        char *endptr = NULL;
        id = (int) strtoll(node->argv[1] + 1, &endptr, 10);
        if (*endptr != 0x00) {
            fprintf(stderr, "fg: Numeric job ID required!\n");
            if (status) *status = 1;
            return 1;
        }
    }
    job *j = get_job(id);
    if (!j) {
        fprintf(stderr, "fg: Job not found!\n");
        if (status) *status = 1;
        return 1;
    }

    j->isbg = 0;
    kill(-j->pgid, SIGCONT);

    if (isatty(STDIN_FILENO) && tcsetpgrp(STDIN_FILENO, j->pgid) == -1)
        perror("execute_cmd: tcsetpgrp");

    j->state = JOB_RUNNING;
    for (int i = 0; i < j->nproc; ++i) {
        if (j->procs[i].state != PROC_DONE) {
            j->procs[i].state = PROC_RUN;
        }
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

    process *last_proc = j->procs + j->nproc - 1;
    if (status) {
        if (last_proc->exit_code != -1) *status = last_proc->exit_code;
        else if (last_proc->term_sig != -1) *status = 128 + last_proc->term_sig;
    }

    // Reclaim the terminal
    if (isatty(STDIN_FILENO) && tcsetpgrp(STDIN_FILENO, getpgrp()) == -1)
        perror("execute_cmd: tcsetpgrp");

    return 0;
}

int jobs_fn(cmd_node *node, int *status) {
    if (!node || !node->argv || !node->argv[0]) return -1;
    print_jobs();
    if (status) *status = 0;
    return 0;
}

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

    if (node->argv[1] == NULL || strcmp(node->argv[1], "~") == 0) {
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

    if (node->io && apply_redir(node, REDIR_TEMPORARY) == -1)
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
