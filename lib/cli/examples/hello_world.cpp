#include <reflex/cli.hpp>

using namespace reflex;

[[
  = cli::specs<"Hello world example">, //
  = cli::specs<":who:", "The one I shall great !">
]] int greater_cli(std::optional<std::string_view> who)
{
  std::println("hello {} !", who.value_or("cli"));
  return 0;
}

int main(int argc, const char** argv)
{
  return cli::run<^^greater_cli>(argc, argv);
}