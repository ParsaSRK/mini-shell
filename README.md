# mini-shell

mini-shell is a small POSIX-like shell written in C. It is a learning-focused project
that implements process creation, job control, redirection, and a custom lexer/parser.

This repo is one of my main resume projects and aims to be clear, readable, and easy
to experiment with.

## Features

- Command execution with `execvp`
- Pipelines (`|`)
- Sequencing (`;`)
- Background operator (`&`) for commands and pipelines
- Logical AND/OR (`&&`, `||`)
- I/O redirection (`<`, `>`, `>>`, with optional FD prefixes like `2>file`)
- Basic job control: `jobs`, `fg`, `bg`, Ctrl+C, Ctrl+Z
- Custom lexer/parser (no external dependencies)

## Build

```sh
make
```

Debug build is the default (ASan enabled). For release:

```sh
make CONFIG=release
```

## Run

```sh
./build/mini-shell
```

## Usage Examples

```sh
echo hello
echo hello | tr a-z A-Z
sleep 2 && echo done
sleep 10 &     # background
jobs
fg %0
bg %0
```

## Builtins

- `cd [dir]`
- `exit [code]`
- `jobs`
- `fg [%id]`
- `bg [%id]`

`fg`/`bg` accept a numeric job id (`%N`). With no argument, they act on the
current job (the most recently added job in the list).

## Job Control Notes

- Each job runs in its own process group.
- Foreground jobs temporarily take the terminal.
- Ctrl+C sends SIGINT to the foreground job.
- Ctrl+Z sends SIGTSTP to the foreground job.

## Limitations

mini-shell is intentionally minimal and does not implement the full POSIX shell spec.
Notable limitations include:

- Background operator only applies to simple commands or pipelines; it does not work
  for `NODE_AND` / `NODE_OR` / sequences (`cmd1 && cmd2 &` is rejected).
- No subshells or grouping (no `(...)`).
- No variable expansion (`$VAR`), command substitution, or arithmetic expansion.
- No globbing (`*`, `?`) or brace expansion.
- No here-docs (`<<`).
- No job control builtins beyond `jobs`, `fg`, `bg`.
- No command history or line editing.
- Job IDs are reused from a fixed pool; they are not monotonic.

## Design Overview

- `src/lex.c`: tokenizes input into operators and words.
- `src/parse.c`: builds a small AST for commands, pipes, and control operators.
- `src/exec.c`: executes the AST, manages process groups, and handles redirections.
- `src/job.c`: tracks jobs and process states for job control.
- `src/builtin.c`: builtin commands (`cd`, `exit`, `jobs`, `fg`, `bg`).

## License

MIT
