#pragma once
#include "parse.h"

/**
 * @brief Signature for builtin command functions.
 *
 * @param node   Command node containing argv and redirections.
 * @param status Optional output status pointer.
 * @return non-zero on error.
 */
typedef int (*builtin_fn)(cmd_node *node, int *status);

/**
 * @brief Builtin dispatch table entry.
 */
typedef struct builtin_cmd {
    const char *name;
    builtin_fn fn;
} builtin_cmd;

/**
 * @brief bg builtin implementation.
 */
int bg_fn(cmd_node *node, int *status);

/**
 * @brief fg builtin implementation.
 */
int fg_fn(cmd_node *node, int *status);

/**
 * @brief jobs builtin implementation.
 */
int jobs_fn(cmd_node *node, int *status);

/**
 * @brief exit builtin implementation.
 */
int exit_fn(cmd_node *node, int *_);

/**
 * @brief cd builtin implementation.
 */
int cd_fn(cmd_node *node, int *status);

/**
 * @brief Check whether a command node is a builtin.
 *
 * @param node Command node to check.
 * @return 1 if builtin, 0 otherwise.
 */
int is_builtin(cmd_node *node);

/**
 * @brief Execute a builtin and apply temporary redirections if needed.
 *
 * @param node Command node to execute.
 * @param status Optional output status pointer.
 * @return 0 on success, non-zero on error.
 */
int run_builtin(cmd_node *node, int *status);
