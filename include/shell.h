#pragma once

typedef struct {
    char* in;
    char* out;
    char* err;
    int argc;
    char** argv;
} command;

int builtin_call(command* cmd);
int builtin_cd(int argc, char** argv);
int builtin_exit(int argc, char** argv);
