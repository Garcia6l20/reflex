# reflex.cli

Command line interface for c++26.

## Example

```cpp
import reflex.cli;

using namespace reflex;

struct[[= cli::command{"Simple echo command."}]] echo
{
  [[= cli::argument{"Message to print."}]] std::string            message;
  [[= cli::option{"-p/--prefix", "Prefix."}]] std::string         prefix;
  [[= cli::option{"-r/--repeat", "Repeat count."}.counter()]] int repeat = 1;

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

## Auto completion support

Auto completion is supported for both bash and zsh. To enable it, you'll have to generate the desired completion script, then source it in your shell configuration:

- For zsh:

```zsh
_REFLEX_COMPLETE=zsh_source foo-bar > foo-bar-complete.zsh
source foo-bar-complete.zsh
```

- For bash:

```bash
_REFLEX_COMPLETE=bash_source foo-bar > foo-bar-complete.bash
source foo-bar-complete.bash
```

Alternatively, you can directly source the output of the generation command:
- For zsh:

```zsh
source <(_REFLEX_COMPLETE=zsh_source foo-bar)
```

- For bash:

```bash
source <(_REFLEX_COMPLETE=bash_source foo-bar)
```

> See [tests](tests) for more examples.
