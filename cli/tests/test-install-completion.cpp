#include <doctest/doctest.h>

#include <reflex/cli.hpp>
#include <testutils.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>

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
// One directory per shell: the cases run in separate processes, and a shared
// directory would be wiped from under whichever case started second.
fs::path temp_home(std::string_view shell)
{
  auto home = fs::temp_directory_path() / ("reflex-install-completion-" + std::string{shell});
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
  auto const home = temp_home("bash");
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
  auto const home = temp_home("zsh");
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
  // The menu style belongs to the completion script, scoped to this command, not to the rc where
  // it would apply to every command the user completes.
  CHECK(rc_text.find("zstyle") == std::string::npos);

  auto const script_text = read_text(completion_path);
  CHECK(script_text.find("compdef _install_cli_completion \"install-cli\"") != std::string::npos);
  CHECK(script_text.find("zstyle ':completion:*:*:install-cli:*' menu select") != std::string::npos);

  // A second install must leave the rc untouched.
  capture_out_err([&] { rc = cli::run(install_completion_cli{}, 3, argv); });
  CHECK_EQ(rc, 0);
  CHECK_EQ(read_text(rc_path), rc_text);

  unset_env("SHELL");
  unset_env("HOME");
  cleanup_home(home);
}

TEST_CASE("reflex::cli: install completion for fish")
{
  auto const home = temp_home("fish");
  auto const home_str = home.string();
  set_env("HOME", home_str.c_str(), true);
  set_env("SHELL", "fish", true);

  int rc = -1;
  const char* argv[] = {"/tmp/install-cli", "--install-completion", "fish"};
  auto [out, err] = capture_out_err([&] { rc = cli::run(install_completion_cli{}, 3, argv); });

  CHECK_EQ(rc, 0);
  CHECK(err.empty());
  CHECK(out.find("fish completion installed in") != std::string::npos);
  CHECK(out.find("Completion will take effect once you restart the terminal") != std::string::npos);

  auto const completion_path = home / ".config/fish/completions/install-cli.fish";
  CHECK(fs::is_regular_file(completion_path));

  auto const script_text = read_text(completion_path);
  CHECK(script_text.find("complete --no-files --command \"install-cli\"") != std::string::npos);

  unset_env("SHELL");
  unset_env("HOME");
  cleanup_home(home);
}

TEST_CASE("reflex::cli: show completion writes the script rather than installing it")
{
  auto const home = temp_home("show");
  auto const home_str = home.string();
  set_env("HOME", home_str.c_str(), true);

  SUBCASE("the shell is named on the command line")
  {
    int rc = -1;
    const char* argv[] = {"/tmp/install-cli", "--show-completion", "zsh"};
    auto [out, err] = capture_out_err([&] { rc = cli::run(install_completion_cli{}, 3, argv); });

    CHECK_EQ(rc, 0);
    CHECK(err.empty());
    CHECK(out.find("compdef _install_cli_completion \"install-cli\"") != std::string::npos);
  }

  SUBCASE("the shell comes from the environment")
  {
    set_env("SHELL", "/bin/bash", true);

    int rc = -1;
    const char* argv[] = {"/tmp/install-cli", "--show-completion"};
    auto [out, err] = capture_out_err([&] { rc = cli::run(install_completion_cli{}, 2, argv); });

    CHECK_EQ(rc, 0);
    CHECK(err.empty());
    CHECK(out.find("complete -o nosort -F _install_cli_completion \"install-cli\"") != std::string::npos);

    unset_env("SHELL");
  }

  SUBCASE("what follows is an option, not a shell name")
  {
    set_env("SHELL", "fish", true);

    int rc = -1;
    const char* argv[] = {"/tmp/install-cli", "--show-completion", "--help"};
    auto [out, err] = capture_out_err([&] { rc = cli::run(install_completion_cli{}, 3, argv); });

    CHECK_EQ(rc, 0);
    CHECK(out.find("complete --no-files --command \"install-cli\"") != std::string::npos);

    unset_env("SHELL");
  }

  SUBCASE("an unknown shell writes nothing")
  {
    int rc = -1;
    const char* argv[] = {"/tmp/install-cli", "--show-completion", "csh"};
    auto [out, err] = capture_out_err([&] { rc = cli::run(install_completion_cli{}, 3, argv); });

    CHECK_EQ(rc, 0);
    CHECK(out.empty());
  }

  // Nothing may have been written to the home directory.
  CHECK(fs::is_empty(home));

  unset_env("HOME");
  cleanup_home(home);
}

TEST_CASE("reflex::cli: installing completion needs a shell it knows")
{
  auto const home = temp_home("unknown-shell");
  auto const home_str = home.string();
  set_env("HOME", home_str.c_str(), true);

  SUBCASE("nothing says which shell is running")
  {
    unset_env("SHELL");

    int rc = -1;
    const char* argv[] = {"/tmp/install-cli", "--install-completion"};
    auto [out, err] = capture_out_err([&] { rc = cli::run(install_completion_cli{}, 2, argv); });

    CHECK_NE(rc, 0);
    CHECK(err.find("cannot detect shell for completion installation") != std::string::npos);
  }

  SUBCASE("the shell is one it has no script for")
  {
    int rc = -1;
    const char* argv[] = {"/tmp/install-cli", "--install-completion", "csh"};
    auto [out, err] = capture_out_err([&] { rc = cli::run(install_completion_cli{}, 3, argv); });

    CHECK_NE(rc, 0);
    CHECK(err.find("shell csh is not supported for completion installation") != std::string::npos);
  }

  CHECK(fs::is_empty(home));

  unset_env("HOME");
  cleanup_home(home);
}

TEST_CASE("reflex::cli: a program reached through PATH is completed by its bare name")
{
  auto const home = temp_home("bare-name");
  auto const home_str = home.string();
  set_env("HOME", home_str.c_str(), true);
  set_env("SHELL", "/bin/bash", true);

  int rc = -1;
  const char* argv[] = {"install-cli", "--install-completion"};
  auto [out, err] = capture_out_err([&] { rc = cli::run(install_completion_cli{}, 2, argv); });

  CHECK_EQ(rc, 0);
  CHECK(err.empty());

  auto const script_text = read_text(home / ".bash_completions/install-cli.sh");
  CHECK(script_text.find("_REFLEX_COMPLETE=bash_complete \"install-cli\"") != std::string::npos);
  CHECK(script_text.find("if [[ -n \"\" && -d \"\" ]]") != std::string::npos);

  unset_env("SHELL");
  unset_env("HOME");
  cleanup_home(home);
}

TEST_CASE("reflex::cli: an rc file that does not end in a newline gets one")
{
  auto const home = temp_home("no-newline");
  auto const home_str = home.string();
  set_env("HOME", home_str.c_str(), true);
  set_env("SHELL", "/bin/bash", true);

  {
    auto out = std::ofstream{home / ".bashrc"};
    out << "export EXISTING=1";
  }

  int rc = -1;
  const char* argv[] = {"/tmp/install-cli", "--install-completion"};
  capture_out_err([&] { rc = cli::run(install_completion_cli{}, 2, argv); });
  CHECK_EQ(rc, 0);

  auto const rc_text = read_text(home / ".bashrc");
  CHECK(rc_text.starts_with("export EXISTING=1\n"));
  CHECK(rc_text.find("\nsource '") != std::string::npos);

  unset_env("SHELL");
  unset_env("HOME");
  cleanup_home(home);
}

TEST_CASE("reflex::cli: with no home directory the completion lands in the current one")
{
  auto const home = temp_home("no-home");
  auto const previous = fs::current_path();
  fs::current_path(home);
  unset_env("HOME");
  set_env("SHELL", "/bin/bash", true);

  int rc = -1;
  const char* argv[] = {"/tmp/install-cli", "--install-completion"};
  capture_out_err([&] { rc = cli::run(install_completion_cli{}, 2, argv); });
  CHECK_EQ(rc, 0);
  CHECK(fs::is_regular_file(home / ".bash_completions/install-cli.sh"));

  fs::current_path(previous);
  unset_env("SHELL");
  cleanup_home(home);
}
