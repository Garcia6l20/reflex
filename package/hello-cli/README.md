# Hello cli example

This example shows how to create a simple command line tool with reflex::cli.

## Building

```bash
$ cd <here>
$ cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
$ cmake --build build -j
```

## Using

```bash
$ build/hello-cli -h
USAGE: hello-cli [OPTIONS...] ARGUMENTS...

hello

OPTIONS:
  -h/--help        Print this message and exit.
  -n/--name        Your name.

$ build/hello-cli
Hello, World!

$ build/hello-cli --name Bob
Hello, Bob!
```

## Autocompletion

```zsh
$ source <(_REFLEX_COMPLETE=auto build/hello-cli) # note: also updates your path to the binary if needed
$ hello-cli -[TAB]
--help  -h  -- Print this message and exit.
--name  -n  -- Your name.
```
