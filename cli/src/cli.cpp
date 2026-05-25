#include <reflex/cli.hpp>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <string>

namespace reflex::cli::detail
{
namespace
{

namespace fs = std::filesystem;

constexpr std::string_view completion_var = "_REFLEX_COMPLETE";

std::string replace_all(std::string text, std::string_view from, std::string_view to)
{
  std::size_t pos = 0;
  while((pos = text.find(from, pos)) != std::string::npos)
  {
    text.replace(pos, from.size(), to);
    pos += to.size();
  }
  return text;
}

std::string sanitize_identifier(std::string_view text)
{
  std::string result;
  result.reserve(text.size());
  for(unsigned char c : text)
  {
    if(c == '-')
    {
      result.push_back('_');
    }
    else if(std::isalnum(c) or c == '_')
    {
      result.push_back(char(c));
    }
  }
  return result;
}

std::string to_lower(std::string_view text)
{
  std::string result{text};
  std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) {
    return char(std::tolower(c));
  });
  return result;
}

fs::path home_path()
{
  if(const auto home = std::getenv("HOME"); home != nullptr and *home != '\0')
  {
    return fs::path{home};
  }
#ifdef _WIN32
  if(const auto userprofile = std::getenv("USERPROFILE");
     userprofile != nullptr and *userprofile != '\0')
  {
    return fs::path{userprofile};
  }
#endif
  return fs::current_path();
}

std::string shell_name(std::string_view shell)
{
  return to_lower(fs::path{std::string{shell}}.filename().generic_string());
}

bool is_supported_shell(std::string_view shell)
{
  auto name = shell_name(shell);
  // TODO support for fish and powershell
  return name == "bash" or name == "zsh";
}

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

  auto input = fs::path{executable, fs::path::format::generic_format};

  auto id = sanitize_identifier(input.filename().generic_string());

  if(not input.has_parent_path())
  {
    // assume it's in the PATH, we dont do anything

    return {
        .program = std::string{executable},
        .dir     = {},
        .id      = std::move(id),
    };
  }
  else
  {
    input = fs::absolute(input);
#ifdef _WIN32
    // create mingw-like path (e.g.: /c/Program Files/foo.exe)
    {
      auto root_name = input.root_name().generic_string();
      if(root_name.size() == 2 and root_name[1] == ':')
      {
        const char drive_letter = to_lower(root_name[0]);
        input                   = fs::relative(input, input.root_path());
        input = fs::path{std::format("/{}", drive_letter)} / input.lexically_normal();
      }
    }
#else
    input = input.lexically_normal();
#endif
    return {
        .program = input.filename().generic_string(),
        .dir     = input.parent_path().generic_string(),
        .id      = std::move(id),
    };
  }
}

std::string completion_script(path_info const& info, std::string_view shell)
{
  auto name = shell_name(shell);
  if(name == "bash")
  {
    static constexpr char __source_template[] = {
#embed "_bash_source.sh"
        , 0};

    return std::format(
        __source_template, info.program, info.id, info.executable_full(), info.dir,
#ifdef _WIN32
        " | tr -d '\r'"
#else
        ""
#endif
    );
  }

  if(name == "zsh")
  {
    static constexpr char __source_template[] = {
#embed "_zsh_source.sh"
        , 0};

    return std::format(__source_template, info.program, info.id, info.executable_full(), info.dir);
  }

  return {};
}

fs::path install_bash(std::string_view executable)
{
  auto const info            = resolve_path(executable);
  auto const home            = home_path();
  auto const rc_path         = home / ".bashrc";
  auto const completion_path = home / ".bash_completions" / (std::string{info.program} + ".sh");
  auto       rc_content      = std::string{};
  auto const source_line     = std::string{"source '"} + completion_path.generic_string() + "'";

  if(fs::is_regular_file(rc_path))
  {
    std::ifstream      in{rc_path};
    std::ostringstream buffer;
    buffer << in.rdbuf();
    rc_content = buffer.str();
  }
  if(rc_content.find(source_line) == std::string::npos)
  {
    if(not rc_content.empty() and rc_content.back() != '\n')
    {
      rc_content.push_back('\n');
    }
    rc_content += source_line;
    rc_content.push_back('\n');
    std::filesystem::create_directories(rc_path.parent_path());
    std::ofstream out{rc_path};
    out.write(rc_content.data(), static_cast<std::streamsize>(rc_content.size()));
  }

  std::filesystem::create_directories(completion_path.parent_path());
  {
    std::ofstream out{completion_path};
    auto          script = completion_script(info, "bash");
    out.write(script.data(), static_cast<std::streamsize>(script.size()));
    out.put('\n');
  }
  return completion_path;
}

fs::path install_zsh(std::string_view executable)
{
  auto const info            = resolve_path(executable);
  auto const home            = home_path();
  auto const rc_path         = home / ".zshrc";
  auto const completion_path = home / ".zfunc" / (std::string{"_"} + std::string{info.program});
  auto const completion_line = std::string{"fpath+=~/.zfunc; autoload -Uz compinit; compinit"};
  auto const style_line      = std::string{"zstyle ':completion:*' menu select"};
  auto       rc_content      = std::string{};

  if(fs::is_regular_file(rc_path))
  {
    std::ifstream      in{rc_path};
    std::ostringstream buffer;
    buffer << in.rdbuf();
    rc_content = buffer.str();
  }
  if(rc_content.find(completion_line) == std::string::npos)
  {
    if(not rc_content.empty() and rc_content.back() != '\n')
    {
      rc_content.push_back('\n');
    }
    rc_content += completion_line;
    rc_content.push_back('\n');
  }
  if(rc_content.find("zstyle") == std::string::npos)
  {
    if(not rc_content.empty() and rc_content.back() != '\n')
    {
      rc_content.push_back('\n');
    }
    rc_content += style_line;
    rc_content.push_back('\n');
  }
  std::filesystem::create_directories(rc_path.parent_path());
  {
    std::ofstream out{rc_path};
    out.write(rc_content.data(), static_cast<std::streamsize>(rc_content.size()));
  }

  std::filesystem::create_directories(completion_path.parent_path());
  {
    std::ofstream out{completion_path};
    auto          script = completion_script(info, "zsh");
    out.write(script.data(), static_cast<std::streamsize>(script.size()));
  }
  return completion_path;
}
} // namespace

void emit_zsh_source(std::string_view executable)
{
  auto const info = resolve_path(executable);
  std::println("{}", completion_script(info, "zsh"));
}

void emit_bash_source(std::string_view executable)
{
  auto const info = resolve_path(executable);
  std::println("{}", completion_script(info, "bash"));
}

int emit_completion(std::string_view executable, std::string_view shell)
{
  auto name = shell_name(shell);
  if(name == "bash")
  {
    emit_bash_source(executable);
  }
  else if(name == "zsh")
  {
    emit_zsh_source(executable);
  }
  return 0;
}

int install_completion(std::string_view executable, std::string_view shell)
{
  auto const  shell_value = shell.empty() ? std::getenv("SHELL") : nullptr;
  std::string shell_name_value;
  if(shell.empty())
  {
    if(shell_value == nullptr)
    {
      std::println(
          std::cerr, "cannot detect shell for completion installation (SHELL env var not set)");
      return 1;
    }
    shell_name_value = shell_name(shell_value);
  }
  else
  {
    shell_name_value = shell_name(shell);
  }

  if(not is_supported_shell(shell_name_value))
  {
    std::println(
        std::cerr, "shell {} is not supported for completion installation", shell_name_value);
    return 1;
  }

  fs::path installed_path;
  if(shell_name_value == "bash")
  {
    installed_path = install_bash(executable);
  }
  else if(shell_name_value == "zsh")
  {
    installed_path = install_zsh(executable);
  }

  std::println("{} completion installed in {}.", shell_name_value, installed_path.generic_string());
  std::println("Completion will take effect once you restart the terminal");
  return 0;
}
} // namespace reflex::cli::detail
