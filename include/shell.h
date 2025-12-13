#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define CMD_COUNT 1

typedef int (*cmd)(int argc, char* argv[]);

typedef struct builtin_cmd {
    char* name;
    cmd fptr;
} builtin_cmd;

extern builtin_cmd commands[CMD_COUNT];

void init_builtin(void);
int call_builtin(int argc, char* argv[]);
int cd(int argc, char* argv[]);
