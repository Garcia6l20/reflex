#pragma once

#ifndef MODULE_MODE
#include <reflex/cli.hpp>
#else
import reflex.cli;
import std;
#endif

using namespace reflex;

auto branch_name_completer(std::string_view current)
{
  using namespace std::string_view_literals;
  static constexpr std::array branches{"main"sv,        "master"sv,      "develop"sv,
                                       "feature/foo"sv, "feature/bar"sv, "hotfix/123"sv};
  return branches
       | std::views::filter(
             [current](std::string_view b) { return current.empty() or b.starts_with(current); })
       | std::views::transform([](std::string_view b) {
           return cli::completion{.value = b, .description = "Available branches"};
         });
}

auto remote_name_completer(std::string_view current)
{
  using namespace std::string_view_literals;
  static constexpr std::array remotes{"origin"sv, "upstream"sv, "fork"sv};
  return remotes
       | std::views::filter(
             [current](std::string_view r) { return current.empty() or r.starts_with(current); })
       | std::views::transform([](std::string_view r) {
           return cli::completion{.value = r, .description = "Available remotes"};
         });
}

struct[[= cli::command{"Git-like command with subcommands."}]] git
{
  [[= cli::option{"-v/--verbose", "Increase verbosity level"}.counter()]] int verbose = 1;

  struct[[= cli::command{"Commit changes."}]]
  {
    git& up;

    [[= cli::argument{"Commit message."}]] std::string message = "";

    int operator()() const
    {
      std::println("Committing with message: {}", message);
      return 0;
    }
  } commit{*this};

  struct[[= cli::command{"Push changes."}]]
  {
    git& up;

    [[= cli::option{"-r/--remote", "Remote name."},
      = cli::complete{^^remote_name_completer}]] std::string remote = "origin";

    int operator()() const
    {
      std::println("Pushing to remote: {}", remote);
      return 0;
    }
  } push{*this};

  struct[[= cli::command{"Manage branches."}]]
  {
    git& up;

    [[= cli::argument{"Branch name."}, = cli::complete{^^branch_name_completer}]] //
        std::string name = "";

    [[= cli::option{"-d/--delete", "Delete the branch."}.flag()]] bool del = false;

    [[= cli::option{"-m/--move", "Rename the branch."}.flag()]] bool move = false;

    int operator()() const
    {
      if(del)
        std::println("Deleting branch: {}", name);
      else if(move)
        std::println("Renaming branch: {}", name);
      else
        std::println("Creating branch: {}", name);
      return 0;
    }
  } branch{*this};

  struct[[= cli::command{"Add one text file to staging."}]]
  {
    git& up;

    [[= cli::argument{"A text file."}, = cli::completers::path{"*.txt"}]] //
        std::string file = "";

    int operator()() const
    {
      std::println("Adding file: {}", file);
      return 0;
    }
  } add_text_file{*this};

  struct[[= cli::command{"Add items to staging."}]]
  {
    git& up;

    [[= cli::argument{"Path specs."}, = cli::completers::path{}]] std::vector<std::string>
        path_specs{};

    // assertion failure: repeated arguments must be last
    // [[= cli::argument{"Invalid"}]] int value = 0;

    int operator()() const
    {
      for(const auto& spec : path_specs)
      {
        std::println("Adding: {}", spec);
      }
      return 0;
    }
  } add{*this};
};
