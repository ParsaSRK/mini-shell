#pragma once

#include "parse.h"

typedef enum apply_redir_mode {
    REDIR_TEMPORARY,
    REDIR_PERMANENTLY
} apply_redir_mode;

/**
 * @brief Apply I/O redirections for a command node.
 *
 * @param node Command node whose redirections should be applied.
 * @param mode REDIR_TEMPORARY to save/restore with undo_redir, or
 *             REDIR_PERMANENTLY for child processes before exec.
 * @return non-zero on error.
 */
int apply_redir(cmd_node *node, apply_redir_mode mode);

/**
 * @brief Restore file descriptors saved by apply_redir(REDIR_TEMPORARY).
 */
void undo_redir(void);
