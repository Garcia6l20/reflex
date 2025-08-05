# reflex.cli

Command line interface for c++26.

## Example

```cpp
#include <reflex/cli.hpp>

using namespace reflex;

[[
  = cli::specs{"hello world example"},
  = cli::specs{":who:", "The one I shall great !"},
  = cli::specs{":verbose:", "-v/--verbose", "Let me talk much more !"}, = cli::_count
]] int greater_cli(std::optional<std::string_view> who, std::optional<int> verbose)
{
  const auto v = verbose.value_or(0);
  const auto w = who.value_or("cli");
  switch(v)
  {
    case 0:
      std::println("hello {} !", w);
      break;
    case 1:
      std::println("hello my dear {} !", w);
      break;
    default:
      std::println("come on !");
      break;
  }
  return 0;
}

int main(int argc, const char** argv)
{
  return cli::run<^^greater_cli>(argc, argv);
}
```

> See [tests](tests) for more examples.
