# mini-shell

A minimal Unix-like shell implemented in C.

This project is a learning exercise focused on understanding process creation, program execution, and basic shell behavior on POSIX systems.

## Features

* Interactive prompt showing the current working directory
* Execution of external commands via `fork`, `execvp`, and `waitpid`
* Built-in commands:

  * `cd`
  * `exit`
* Proper handling of exit codes and signals
* Clean memory management

## Build

Requires a POSIX-compatible system.

Build the project with:

```
make CONFIG=release
```

For a debug build (default):
```
make CONFIG=debug
```

The binary will be created at:

```
build/mini-shell
```

## Run

```
./build/mini-shell
```

## Limitations

* No pipes
* No job control
* No quoting or escaping in input parsing

## License

MIT
