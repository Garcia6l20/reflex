#include <doctest/doctest.h>

#include <reflex/cli.hpp>
#include <testutils.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

using namespace reflex;
using namespace testutils;

namespace fs = std::filesystem;

struct [[= cli::command{"Install completion test."}]] install_completion_cli
{
  int operator()() const
  {
    return 0;
  }
};

namespace
{
fs::path temp_home()
{
  auto home = fs::temp_directory_path() / "reflex-install-completion";
  fs::remove_all(home);
  fs::create_directories(home);
  return home;
}

void cleanup_home(fs::path const& home)
{
  fs::remove_all(home);
}

std::string read_text(fs::path const& path)
{
  auto in = std::ifstream{path};
  return std::string{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
}
} // namespace

TEST_CASE("reflex::cli: install completion for bash")
{
  auto const home = temp_home();
  auto const home_str = home.string();
  set_env("HOME", home_str.c_str(), true);
  set_env("SHELL", "/bin/bash", true);

  int rc = -1;
  const char* argv[] = {"/tmp/install-cli", "--install-completion"};
  auto [out, err] = capture_out_err([&] { rc = cli::run(install_completion_cli{}, 2, argv); });

  CHECK_EQ(rc, 0);
  CHECK(err.empty());
  CHECK(out.find("bash completion installed in") != std::string::npos);
  CHECK(out.find("Completion will take effect once you restart the terminal") != std::string::npos);

  auto const rc_path = home / ".bashrc";
  auto const completion_path = home / ".bash_completions/install-cli.sh";
  CHECK(fs::is_regular_file(rc_path));
  CHECK(fs::is_regular_file(completion_path));

  auto const rc_text = read_text(rc_path);
  auto const expected_source = "source '" + completion_path.string() + "'";
  CHECK(rc_text.find(expected_source) != std::string::npos);

  auto const script_text = read_text(completion_path);
  CHECK(script_text.find("complete -o nosort -F _install_cli_completion \"install-cli\"") != std::string::npos);

  unset_env("SHELL");
  unset_env("HOME");
  cleanup_home(home);
}

TEST_CASE("reflex::cli: install completion for zsh")
{
  auto const home = temp_home();
  auto const home_str = home.string();
  set_env("HOME", home_str.c_str(), true);
  set_env("SHELL", "zsh", true);

  int rc = -1;
  const char* argv[] = {"/tmp/install-cli", "--install-completion", "zsh"};
  auto [out, err] = capture_out_err([&] { rc = cli::run(install_completion_cli{}, 3, argv); });

  CHECK_EQ(rc, 0);
  CHECK(err.empty());
  CHECK(out.find("zsh completion installed in") != std::string::npos);
  CHECK(out.find("Completion will take effect once you restart the terminal") != std::string::npos);

  auto const rc_path = home / ".zshrc";
  auto const completion_path = home / ".zfunc/_install-cli";
  CHECK(fs::is_regular_file(rc_path));
  CHECK(fs::is_regular_file(completion_path));

  auto const rc_text = read_text(rc_path);
  CHECK(rc_text.find("fpath+=~/.zfunc; autoload -Uz compinit; compinit") != std::string::npos);

  auto const script_text = read_text(completion_path);
  CHECK(script_text.find("compdef _install_cli_completion \"install-cli\"") != std::string::npos);

  unset_env("SHELL");
  unset_env("HOME");
  cleanup_home(home);
}
