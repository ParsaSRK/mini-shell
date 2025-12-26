#pragma once

/**
* Free a NULL-terminated array of pointers.
 *
 * @param arr     Heap-allocated, NULL-terminated pointer array.
 * @param destroy Destructor for one element, or NULL.
 */
void free_ptrv(void **arr, void (*destroy)(void *));