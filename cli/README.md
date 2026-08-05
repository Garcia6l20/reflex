# reflex.cli

> **command-line interfaces for C++26**, powered by static reflection.

Define your CLI as an annotated struct, or as an annotated function.  `reflex.cli`
derives the parser, help text, and shell completions automatically.

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
  --help                Print this message and exit.
  --install-completion  Install shell completion.
  --show-completion     Show shell completion.
  -p/--prefix           Prefix.
  -r/--repeat           Repeat count.

ARGUMENTS:
  message          Message to print.
```

---

## Functions and callables

A command can be a plain function instead of a struct.  Annotate the parameters
rather than the members and pass the function's reflection to `cli::run`.

```cpp
[[= cli::command{"Simple echo command."}]]
int echo(
    [[= cli::argument{"Message to print."}]] std::string message,
    [[= cli::option{"-p/--prefix", "Prefix."}]] std::string prefix,
    [[= cli::option{"-r/--repeat", "Repeat count."}]] int repeat)
{
  for(auto _ : std::views::iota(0, std::max(repeat, 1)))
  {
    if(!prefix.empty())
      std::print("{}: ", prefix);
    std::println("{}", message);
  }
  return 0;
}

int main(int argc, const char** argv)
{
  return cli::run<^^echo>(argc, argv);
}
```

The help text, the parsing and the completions are the same as for a struct: the
parameters are described as the members of a synthesized aggregate and the
ordinary parser runs on that.

A lambda or a function object works the same way, named through the variable or
the type that holds it.  A capturing lambda keeps its captures, and a function
object type is default constructed.

```cpp
[[= cli::command{"Print a line of stars."}]]
constexpr auto stars = []([[= cli::argument{"How many."}]] int count) { … };

cli::run<^^stars>(argc, argv);
```

### What a function command cannot do

| Refused | Why, and what to write instead |
|---|---|
| a defaulted parameter | there is nowhere to keep the default. Use `std::optional<T>` |
| a reference parameter | a command line has nothing to bind it to |
| a sub-command parameter | a function is a leaf, use a nested struct for an interior command |
| a generic lambda | a templated call operator has no parameter types to read |
| a return type that is neither `void` nor convertible to `int` | what a command returns is what the process returns |

---

## Annotations

### `cli::command`

Marks a struct as a (sub)command and provides its help text.  It marks a
function, a lambda variable or a function object type the same way.

```cpp
struct [[= cli::command{"Does something useful."}]] my_cmd { … };

[[= cli::command{"Does something useful."}]] int my_cmd(…);
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

A sub-command may also be a **member function**, annotated the same way, with
its parameters annotated instead of a nested struct's members.  It is called on
the command that declares it, so the parent's options are simply in scope.

```cpp
struct [[= cli::command{"Git-like tool."}]] git
{
  [[= cli::option{"-v/--verbose", "Verbose output."}]] int verbose = 0;

  [[= cli::command{"Commit staged changes."}]]
  int commit([[= cli::argument{"Commit message."}]] std::string message)
  {
    if(verbose > 0)                       // the parent's option, no plumbing
      std::println("committing…");
    std::println("{}", message);
    return 0;
  }
};
```

With a nested struct the same access costs a back-reference member and its
initializer, `git& up;` and `commit{*this}`, on every sub-command that wants it.

Both kinds may be mixed in one command, and they appear in `--help` in
declaration order.

#### A member function sub-command is always a leaf

Its parameters become an aggregate, and an aggregate holds no sub-commands, so
`git remote add <name>` still needs a **nested struct** at the `remote` level.
Design the tree before choosing the form.  A parameter that would become a
sub-command is refused at compile time, not ignored.

| Refused | Why |
|---|---|
| a parameter that is itself a sub-command | a member function is a leaf, use a nested struct |
| a non-public member function | the parser reaches a sub-command from outside the command |
| two annotated overloads | they share one name, so only the first could be reached |

A member function **template** cannot be a sub-command and cannot be diagnosed
either, so it is silently not one: `annotations_of` accepts a function but not a
function template and throws on one, so the annotation is unreachable.

#### Ordering

A parent's options are parsed before the descent, so they go before the
sub-command name.  This applies to nested structs and member functions alike.

```
git -v commit "msg"     # correct
git commit -v "msg"     # -v is not the parent's here
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
[[= cli::argument{"Any path."},    = cli::completers::path{}]]            std::string path;
[[= cli::argument{"A directory."}, = cli::completers::path<>::dirs{}]]    std::string dir;
[[= cli::argument{"Any file."},    = cli::completers::path<>::files{}]]   std::string file;
[[= cli::argument{"A JSON file."}, = cli::completers::path{"*.json"}]]    std::string cfg;
```

---

## Shell completion

### Installation

```bash
# zsh
my-tool --install-completion zsh

# bash
my-tool --install-completion bash

# auto detect (uses $SHELL environment variable)
my-tool --install-completion
```

### Inline sourcing

```zsh
source <(./path/to/my-tool --show-completion zsh)
source <(./path/to/my-tool --show-completion bash)
source <(./path/to/my-tool --show-completion) # auto detect, uses $SHELL environment variable
# NOTE: this also updates your path to the binary if needed
```

### Example (from [/package/hello-cli](../package/hello-cli))

```zsh
$ cd package/hello-cli
$ cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
$ cmake --build build -j
$ source <(build/hello-cli --show-completion) # note: also updates your path to the binary
$ hello-cli -[TAB]
--help  -h  -- Print this message and exit.
--name  -n  -- Your name.
```

### Completion protocol

| Variable | Description |
|---|---|
| `_REFLEX_COMPLETE`   | Mark a completion request |
| `_REFLEX_COMP_LINE`  | Full command line typed so far |
| `_REFLEX_COMP_POINT` | Word index of the token being completed |

---

> See [tests](tests) for more examples.