#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "shell.h"

static int apply_io(command* cmd) {
    // Init file descriptors
    int fdout = -1;
    int fdin = -1;
    int fderr = -1;

    // Open all file descriptors
    if (cmd->out != NULL) {
        fdout = open(cmd->out, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fdout == -1) {
            perror("open outfile");
            goto error;
        }
    }
    if (cmd->in != NULL) {
        fdin = open(cmd->in, O_RDONLY);
        if (fdin == -1) {
            perror("open infile");
            goto error;
        }
    }
    if (cmd->err != NULL) {
        fderr = open(cmd->err, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fderr == -1) {
            perror("open errfile");
            goto error;
        }
    }

    if (fdout != -1 && dup2(fdout, STDOUT_FILENO) == -1) goto error;
    if (fdin != -1 && dup2(fdin, STDIN_FILENO) == -1) goto error;
    if (fderr != -1 && dup2(fderr, STDERR_FILENO) == -1) goto error;

    if (fdout != -1) close(fdout);
    if (fdin != -1) close(fdin);
    if (fderr != -1) close(fderr);
    return 0;

error:
    if (fdout != -1) close(fdout);
    if (fdin != -1) close(fdin);
    if (fderr != -1) close(fderr);
    return 1;
}

static void command_free(command* cmd) {
    free(cmd->argv);
    free(cmd);
}

static command* parse(char* line) {
    command* cmd = calloc(1, sizeof(command));
    if (cmd == NULL) {
        perror("calloc");
        return NULL;
    }

    // Tokenization
    for (char* token = strtok(line, " \n"); token != NULL;
         token = strtok(NULL, " \n")) {
        // I/O redir check
        if (strcmp(token, ">") == 0) {
            token = strtok(NULL, " \n");
            if (token == NULL) {
                command_free(cmd);
                return NULL;
            }
            cmd->out = token;
        } else if (strcmp(token, "<") == 0) {
            token = strtok(NULL, " \n");
            if (token == NULL) {
                command_free(cmd);
                return NULL;
            }
            cmd->in = token;
        } else if (strcmp(token, "2>") == 0) {
            token = strtok(NULL, " \n");
            if (token == NULL) {
                command_free(cmd);
                return NULL;
            }
            cmd->err = token;
        } else {  // It is argv
            // Reallocate memory for argv safely
            char** tmp = realloc(cmd->argv, (cmd->argc + 2) * sizeof(char*));
            if (tmp == NULL) {
                perror("realloc");
                command_free(cmd);
                return NULL;
            }
            cmd->argv = tmp;
            cmd->argv[cmd->argc++] = token;
        }
    }
    if (cmd->argc == 0) {
        command_free(cmd);
        return NULL;
    }
    cmd->argv[cmd->argc] = NULL;
    return cmd;
}

static int execute(command* cmd) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return -1;
    }
    if (pid == 0) {  // Child branch
        if (apply_io(cmd) != 0) _exit(127);
        execvp(cmd->argv[0], cmd->argv);
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

int main(void) {
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

        // Parse
        command* cmd = parse(line);
        if (cmd == NULL) {
            free(line);
            continue;
        }

        int ret;

        // Check if it is builtin command
        ret = builtin_call(cmd);
        if (ret != -1) {  // it was builtin command
            command_free(cmd);
            free(line);
            continue;
        }

        // Try to find in PATH, make subprocess and execute it
        ret = execute(cmd);

        // Free the line
        command_free(cmd);
        free(line);
    }
    return 0;
}
