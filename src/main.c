#include "shell.h"

int main(int argc, char* argv[]) {
    init_builtin();
    while (1) {
        // Find CWD
        char* cwd = getcwd(NULL, 0);
        if (cwd == NULL) {
            perror("getcwd");
            return 1;
        }

        // Print prompt
        printf("%s > ", cwd);
        fflush(stdout);
        free(cwd);

        // Get a line
        char* line = NULL;
        size_t cap = 0;
        ssize_t n = getline(&line, &cap, stdin);
        if (n == -1) {
            free(line);
            if (feof(stdin)) {
                printf("\n");
                break;
            } else {
                perror("getline");
                continue;
            }
        }

        // Parse line to token
        size_t child_argc = 0;
        char** child_argv = NULL;
        char* token = strtok(line, " \n");  // Tokenize the string
        while (token != NULL) {
            // Reallocate memory for child_argv safely
            char** tmp = realloc(child_argv, (child_argc + 2) * sizeof(char*));
            if (tmp == NULL) {
                perror("realloc");
                free(child_argv);
                free(line);
                return 1;
            }
            child_argv = tmp;

            child_argv[child_argc++] = token;
            token = strtok(NULL, " \n");  // Get next token
        }
        if (child_argc == 0) {
            free(child_argv);
            free(line);
            continue;
        }
        child_argv[child_argc] = NULL;

        // Todo: check if builtin
        int ret = call_builtin(child_argc, child_argv);

        // Free the line
        free(child_argv);
        free(line);
    }
    return 0;
}
