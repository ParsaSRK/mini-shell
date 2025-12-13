#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "shell.h"

typedef int (*cmd_fn)(int argc, char** argv);

typedef struct {
    const char* name;
    cmd_fn fptr;
} builtin_cmd;

static builtin_cmd commands[] = {
    {"cd", builtin_cd}, {"exit", builtin_exit}, {NULL, NULL}};

int builtin_cd(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "usage: cd <DIR>\n");
        return 1;
    }

    if (chdir(argv[1]) != 0) {
        perror("cd");
        return 1;
    }

    return 0;
}

int builtin_exit(int argc, char* argv[]) {
    if (argc <= 1) exit(0);

    char* end;
    long code = strtol(argv[1], &end, 10);

    if (*end != 0x00) {
        fprintf(stderr, "exit: numeric argument required\n");
        exit(2);
    }
    exit(code & 0xFF);
}

int builtin_call(int argc, char* argv[]) {
    for (builtin_cmd* cmdptr = commands; cmdptr->name != NULL; ++cmdptr) {
        if (strcmp(cmdptr->name, argv[0]) == 0)
            return (cmdptr->fptr)(argc, argv);
    }
    return -1;
}
