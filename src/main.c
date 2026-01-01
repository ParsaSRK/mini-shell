#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#include "exec.h"
#include "parse.h"

void init_signals(void) {
    signal(SIGINT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
}

int main(void) {
    init_signals();
    while (1) {
        // Find CWD
        char *cwd = getcwd(NULL, 0);
        if (cwd == NULL) {
            perror("main: getcwd");
            return 1;
        }

        // Print prompt
        printf("%s> ", cwd);
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
            perror("main: getline");
            continue;
        }

        ast_node *root = parse_line(line);

        print_ast(root, 0);

        int status = 0;
        execute_ast(root, &status);
        printf("Exit code: %d\n", status);

        free_ast_node(root);
        free(line);
    }
    return 0;
}
