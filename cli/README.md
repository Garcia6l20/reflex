# reflex.cli

> **command-line interfaces for C++26**, powered by static reflection.

Define your CLI entirely as an annotated struct.  `reflex.cli` derives the parser,
help text, and shell completions automatically.

No code generation, no macros, no registration calls.

---

## Quick start

```cpp
import reflex.cli;
using namespace reflex;

struct [[= cli::command{"Simple echo command."}]] echo
{
  [[= cli::argument{"Message to print."}]]
  std::string message;

  [[= cli::option{"-p/--prefix", "Prefix."}]]
  std::string prefix;

  [[= cli::option{"-r/--repeat", "Repeat count."}.counter()]]
  int repeat = 1;

  int operator()() const
  {
    for(auto _ : std::views::iota(0, repeat))
    {
      if(!prefix.empty())
        std::print("{}: ", prefix);
      std::println("{}", message);
    }
    return 0;
  }
};

int main(int argc, const char** argv)
{
  return cli::run(echo{}, argc, argv);
}
```

```
$ echo --help
USAGE: echo [OPTIONS...] ARGUMENTS...

Simple echo command.

OPTIONS:
  -h/--help        Print this message and exit.
  -p/--prefix      Prefix.
  -r/--repeat      Repeat count.

ARGUMENTS:
  message          Message to print.
```

---

## Annotations

### `cli::command`

Marks a struct as a (sub)command and provides its help text.

```cpp
struct [[= cli::command{"Does something useful."}]] my_cmd { … };
```

### `cli::argument`

A positional argument.  The member type drives parsing.
Use `std::optional<T>` to make it optional.  Use `std::vector<T>` for
a repeated/variadic argument (must be last).

```cpp
[[= cli::argument{"Input file."}]] std::string input;
[[= cli::argument{"Extra files."}]] std::vector<std::string> extras;
[[= cli::argument{"Optional tag."}]] std::optional<std::string> tag;
```

### `cli::option`

A named switch.  The `switches` string uses `/` to separate the short and long
forms: `"-f/--flag"`.  If only a long option is needed write `"--flag"`.

| Modifier | Effect |
|---|---|
| `.flag()` | Maps to `bool`; presence sets it to `true` |
| `.counter()` | Maps to `int`/`std::optional<int>`; `-vvv` → `3` |

```cpp
[[= cli::option{"-v/--verbose", "Verbosity."}.counter()]] int verbose = 0;
[[= cli::option{"-q/--quiet",   "Suppress output."}.flag()]] bool quiet = false;
[[= cli::option{"-o/--output",  "Output file."}]] std::string output;
```

### Sub-commands

Nest annotated structs as members.  Each nested struct becomes a sub-command.

```cpp
struct [[= cli::command{"Git-like tool."}]] git
{
  struct [[= cli::command{"Commit staged changes."}]]
  {
    [[= cli::argument{"Commit message."}]] std::string message;
    int operator()() const { … }
  } commit;

  struct [[= cli::command{"Push to remote."}]]
  {
    [[= cli::option{"-r/--remote", "Remote name."}]] std::string remote = "origin";
    int operator()() const { … }
  } push;
};
```

---

## Custom completers

Attach a completer function to any argument or option with `cli::complete`.
The function receives the current word prefix and returns a range of `cli::completion`.

```cpp
auto branch_completer(std::string_view current)
{
  static constexpr std::array branches{"main"sv, "develop"sv, "feature/foo"sv};
  return branches
       | std::views::filter([current](auto b){ return b.starts_with(current); })
       | std::views::transform([](auto b){
           return cli::completion<>{.value = std::string(b), .description = "branch"};
         });
}

struct [[= cli::command{"Manage branches."}]] branch_cmd
{
  [[= cli::argument{"Branch name."}, = cli::complete{^^branch_completer}]]
  std::string name;
  …
};
```

### Built-in path completers

```cpp
[[= cli::argument{"Any path."},    = cli::completers::path{}]]         std::string path;
[[= cli::argument{"A directory."}, = cli::completers::path::dirs{}]]   std::string dir;
[[= cli::argument{"Any file."},    = cli::completers::path::files{}]]  std::string file;
[[= cli::argument{"A JSON file."}, = cli::completers::path{"*.json"}]] std::string cfg;
```

---

## Shell completion

### One-time script generation

```bash
# zsh
_REFLEX_COMPLETE=zsh_source my-tool > ~/.zsh/completions/_my-tool
source ~/.zsh/completions/_my-tool

# bash
_REFLEX_COMPLETE=bash_source my-tool > ~/.bash_completion.d/my-tool
source ~/.bash_completion.d/my-tool

# auto detect and source on each run
_REFLEX_COMPLETE=auto my-tool > ~/.$SHELL_completion.d/my-tool
source ~/.$SHELL_completion.d/my-tool
```

### Inline sourcing

```zsh
source <(_REFLEX_COMPLETE=zsh_source my-tool)   # zsh
source <(_REFLEX_COMPLETE=bash_source my-tool)  # bash
source <(_REFLEX_COMPLETE=auto my-tool)         # auto
```

### Example (from [/package/hello-cli](../package/hello-cli))

```zsh
$ cd package/hello-cli
$ cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
$ cmake --build build -j
$ source <(_REFLEX_COMPLETE=auto build/hello-cli) # note: also updates your path to the binary if needed
$ hello-cli -[TAB]
--help  -h  -- Print this message and exit.
--name  -n  -- Your name.
```

### Completion protocol

| Variable | Description |
|---|---|
| `_REFLEX_COMPLETE` | `zsh_complete`, `bash_complete`, `zsh_source`, `bash_source` |
| `_REFLEX_COMP_LINE` | Full command line typed so far |
| `_REFLEX_COMP_POINT` | Word index of the token being completed |

---

> See [tests](tests) for more examples.