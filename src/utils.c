#include "utils.h"

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
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

int apply_redir(cmd_node *node) {
    // validate node
    if (!node || !node->io) {
        fprintf(stderr, "apply_redir: Invalid node!\n");
        return -1;
    }
    if (backup != NULL) {
        fprintf(stderr, "apply_redir: Previous redir not undo yet!\n");
        return -1;
    }

    // Count redirections;
    cnt = 0;
    for (redir **it = node->io; *it != NULL; ++it)
        ++cnt;

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

    // Backup fd
    for (redir **it = node->io; *it != NULL; ++it) {
        int saved_fd = dup((*it)->fd);
        if (saved_fd == -1 && errno != EBADF) {
            perror("apply_redir: dup");
            goto cleanup;
        }
        // Mark save_fd as close on execvp (if its valid)
        if (saved_fd != -1 && fcntl(saved_fd, F_SETFD, FD_CLOEXEC) == -1) {
            perror("fcntl(FD_CLOEXEC)");
            goto cleanup;
        }
        backup[it - node->io].saved_fd = saved_fd;
        backup[it - node->io].fd = (*it)->fd;

        int flags = 0;
        if ((*it)->type == REDIR_IN) flags = O_RDONLY;
        else if ((*it)->type == REDIR_OUT) flags = O_WRONLY | O_CREAT | O_TRUNC;
        else flags = O_WRONLY | O_CREAT | O_APPEND;

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
    undo_redir();
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
