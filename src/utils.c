#include "utils.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void free_ptrv(void **arr, void (*destroy)(void *)) {
    if (arr) {
        for (void **i = arr; *i != NULL; ++i)
            if (destroy) destroy(*i);
        free(arr);
    }
}

typedef struct backup {
    int saved_fd;
    int fd;
} fd_pair;

static fd_pair *backup = NULL;
static int cnt = 0;

int apply_redir(cmd_node *node, apply_redir_mode mode) {
    // Validate node
    if (!node) {
        fprintf(stderr, "apply_redir: Invalid node!\n");
        return -1;
    }

    // No redir to apply
    if (!node->io) return 0;

    // Make backup table if REDIR_TEMPORARY
    if (mode == REDIR_TEMPORARY) {
        if (backup != NULL) {
            fprintf(stderr, "apply_redir: Previous redir not undo yet!\n");
            return -1;
        }

        // Count redirections;
        cnt = 0;
        for (redir **it = node->io; *it != NULL; ++it) ++cnt;

        if (!cnt) return 0;

        // Allocate array
        backup = malloc(cnt * sizeof(fd_pair));
        if (!backup) {
            perror("apply_redir: malloc");
            return -1;
        }
        for (int i = 0; i < cnt; ++i) {
            backup[i].saved_fd = -1;
            backup[i].fd = -1;
        }
    }

    // Backup fd
    for (redir **it = node->io; *it != NULL; ++it) {
        // Fill backup table if REDIR_TEMPORARY
        if (mode == REDIR_TEMPORARY) {
            int saved_fd = dup((*it)->fd);
            if (saved_fd == -1 && errno != EBADF) {
                perror("apply_redir: dup");
                goto cleanup;
            }
            backup[it - node->io].saved_fd = saved_fd;
            backup[it - node->io].fd = (*it)->fd;
        }

        // Apply redirections
        int flags = 0;
        if ((*it)->type == REDIR_IN)
            flags = O_RDONLY;
        else if ((*it)->type == REDIR_OUT)
            flags = O_WRONLY | O_CREAT | O_TRUNC;
        else
            flags = O_WRONLY | O_CREAT | O_APPEND;

        int file = open((*it)->path, flags, 0644);
        if (file == -1) {
            perror("apply_redir: open");
            goto cleanup;
        }
        if (dup2(file, (*it)->fd) == -1) {
            perror("dup2");
            close(file);
            goto cleanup;
        }
        if (file != (*it)->fd) close(file);
    }

    return 0;
cleanup:
    if (mode == REDIR_TEMPORARY) undo_redir();
    return -1;
}

void undo_redir(void) {
    if (!backup) return;
    for (int i = cnt - 1; i >= 0; --i) {
        if (backup[i].saved_fd != -1) {
            if (dup2(backup[i].saved_fd, backup[i].fd) == -1) {
                perror("undo_redir: dup2");
            }
            close(backup[i].saved_fd);
        } else if (backup[i].fd != -1) {
            close(backup[i].fd);
        }
    }
    free(backup);
    backup = NULL;
    cnt = 0;
}

void init_signals(void) {
    signal(SIGINT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
}

void reset_signals(void) {
    signal(SIGINT, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
    signal(SIGTTOU, SIG_DFL);
    signal(SIGTTIN, SIG_DFL);
}
