#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>

#include "exec.h"
#include "job.h"
#include "parse.h"

/**
 * @brief Dummy signal handler to cause syscalls fail with EINTR to reset shell.
 */
static void on_sigchild(int signo) {
    (void) signo;
}

int main(void) {
    struct sigaction sa = {0};
    sa.sa_handler = on_sigchild;
    sa.sa_flags = SA_NOCLDSTOP;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGCHLD, &sa, NULL);

    signal(SIGINT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    atexit(kill_jobs);

    while (1) {
        // Update and cleanup job table
        pid_t pid = 0;
        int wstat;
        do {
            pid = waitpid(-1, &wstat, WNOHANG | WUNTRACED | WCONTINUED);
            if (pid > 0) update_proc(pid, wstat);
        } while (pid > 0 || (pid == -1 && errno == EINTR));
        update_jobs();
        remove_zombies();

        // Find CWD
        char *cwd = getcwd(NULL, 0);
        if (cwd == NULL) {
            perror("main: getcwd");
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
            if (errno == EINTR) {
                clearerr(stdin);
                printf("\n");
                continue;
            }

            if (feof(stdin)) {
                printf("\n");
                break;
            }
            perror("main: getline");
            break;
        }

        // Parse input
        ast_node *root = parse_line(line);

        // Print the tree
        // print_ast(root, 0);

        // Print exit code
        int status = 0;
        execute_ast(root, &status, 0);
        if (status != 0) printf("Exit code: %d\n", status);

        // Cleanup
        free_ast_node(root);
        free(line);
    }
    return 0;
}
