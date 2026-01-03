#pragma once

#include "parse.h"

typedef enum redir_mode {
    REDIR_TEMPORARY,
    REDIR_PERMANENTLY
} apply_redir_mode;

/**
 * @brief Free a NULL-terminated array of pointers.
 *
 * @param arr     Heap-allocated, NULL-terminated pointer array.
 * @param destroy Destructor for one element, or NULL.
 */
void free_ptrv(void **arr, void (*destroy)(void *));

int apply_redir(cmd_node *node, apply_redir_mode mode);

void undo_redir(void);

void init_signals(void);

void reset_signals(void);
