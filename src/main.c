#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "shell.h"

static int execute(char** argv) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return -1;
    }
    if (pid == 0) {  // Child branch
        execvp(argv[0], argv);
        perror("execvp");
        _exit(127);
    }

    int status;
    while (waitpid(pid, &status, 0) == -1) {
        if (errno == EINTR) continue;
        perror("waitpid");
        return -1;
    }

    if (WIFEXITED(status))  // exited normally
        return WEXITSTATUS(status);
    else if (WIFSIGNALED(status)) {  // exited with signal
        int sig = WTERMSIG(status);
        fprintf(stderr, "terminated by signal %d(%s)\n", sig, strsignal(sig));
        return -1;
    }
    return -1;  // should not happen
}

int main(int argc, char* argv[]) {
    while (1) {
        // Find CWD
        char* cwd = getcwd(NULL, 0);
        if (cwd == NULL) {
            perror("getcwd");
            return 1;
        }

        // Print prompt
        printf("%s> ", cwd);
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

        int ret;

        // Check if it is builtin command
        ret = builtin_call(child_argc, child_argv);
        if (ret != -1) {  // it was builtin command
            free(child_argv);
            free(line);
            continue;
        }

        // Try to find in PATH, make subprocess and execute it
        ret = execute(child_argv);

        // Free the line
        free(child_argv);
        free(line);
    }
    return 0;
}
