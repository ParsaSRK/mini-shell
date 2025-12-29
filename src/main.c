#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "parse.h"


int main(void) {
    while (1) {
        // Find CWD
        char *cwd = getcwd(NULL, 0);
        if (cwd == NULL) {
            perror("getcwd");
            return 1;
        }

        // Print prompt
        printf("%s> ", cwd);
        fflush(stdout);
        free(cwd);

        // Get a line
        char *line = NULL;
        size_t cap = 0;
        ssize_t n = getline(&line, &cap, stdin);
        if (n == -1) {
            free(line);
            if (feof(stdin)) {
                printf("\n");
                break;
            }
            perror("getline");
            continue;
        }

        ast_node *root = parse_line(line);

        print_ast(root, 0);

        free_ast_node(root);
        free(line);
    }
    return 0;
}
