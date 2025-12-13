#include "shell.h"

builtin_cmd commands[CMD_COUNT];

int cd(int argc, char* argv[]) {
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

void init_builtin(void) {
    builtin_cmd builtin_cd = {.name = "cd", .fptr = cd};
    commands[0] = builtin_cd;
}

int call_builtin(int argc, char* argv[]) {
    for (int i = 0; i < CMD_COUNT; ++i) {
        if (strcmp(commands[i].name, argv[0]) != 0) continue;
        return (*commands[i].fptr)(argc, argv);
    }
    return -1;
}
