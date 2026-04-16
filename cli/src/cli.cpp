#include <reflex/cli.hpp>

#include <filesystem>
#include <format>
#include <string>

namespace reflex::cli::detail
{
namespace
{

namespace fs = std::filesystem;

struct path_info
{
  std::string program;
  std::string dir;
  std::string id;

  auto executable_full() const -> std::string
  {
    if(dir.empty())
    {
      return program;
    }
    return std::format("{}/{}", dir, program);
  }
};

path_info resolve_path(std::string_view executable)
{
  if(executable.empty())
  {
    return {};
  }

  auto input = fs::path{std::string{executable}, fs::path::format::generic_format};

  auto id = input.stem().generic_string();
  std::ranges::replace(id, '-', '_');

  if(not input.has_parent_path())
  {
    // assume it's in the PATH, we dont do anything
    return {
        .program = std::string{executable},
        .dir     = {},
        .id      = id,
    };
  }
  else
  {
    input = input.lexically_normal();
    return {
        .program = input.filename().generic_string(),
        .dir     = fs::absolute(input).parent_path().generic_string(),
        .id      = id,
    };
  }
}
} // namespace

void emit_zsh_source(std::string_view executable)
{
  auto info = resolve_path(executable);

  static constexpr char __source_template[] = {
#embed "_zsh_source.sh"
      , 0};

  std::println(__source_template, info.program, info.id, info.executable_full(), info.dir);
}

void emit_bash_source(std::string_view executable)
{
  auto info = resolve_path(executable);

  static constexpr char __source_template[] = {
#embed "_bash_source.sh"
      , 0};

  std::println(__source_template, info.program, info.id, info.executable_full(), info.dir);
}
} // namespace reflex::cli::detail
