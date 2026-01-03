#pragma once

#include "parse.h"




/**
 * @brief Executes a NODE_CMD and returns its exit code.
 *
 * @param node NODE_CMD to be run.
 * @param status shell-style exit code if successfully executed.
 * @return non-zero if failed (internal error).
 */
int execute_cmd(ast_node *node, int *status);

/**
 * @brief Executes a NODE_SEQ and returns the last command's exit code.
 *
 * Commands are executed in order. Non-zero exit codes do not stop the sequence.
 *
 * @param node NODE_SEQ to be run.
 * @param status shell-style exit code of the last command if successfully executed.
 * @return non-zero if failed (internal error).
 */
int execute_seq(ast_node *node, int *status);

/**
 * @brief Executes a NODE_AND and returns the exit code of the last executed child.
 *
 * Right child executes only if left child returns exit status 0.
 *
 * @param node NODE_AND to be run.
 * @param status shell-style exit code if successfully executed.
 * @return non-zero if failed (internal error).
 */
int execute_and(ast_node *node, int *status);

/**
 * @brief Executes a NODE_OR and returns the exit code of the last executed child.
 *
 * Right child executes only if left child returns a non-zero exit status.
 *
 * @param node NODE_OR to be run.
 * @param status shell-style exit code if successfully executed.
 * @return non-zero if failed (internal error).
 */
int execute_or(ast_node *node, int *status);

/**
 * @brief Dispatches execution based on node type.
 *
 * @param node AST node to be run.
 * @param status shell-style exit code (0-255) if successfully executed.
 * @return non-zero if failed (internal error).
 */
int execute_ast(ast_node *node, int *status);
