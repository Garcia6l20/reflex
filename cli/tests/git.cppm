export module git;

import reflex.cli;
import std;
import std.compat;

using namespace reflex;

export {
  struct[[= cli::command{"Git-like command with subcommands."}]] git
  {
    [[= cli::option{"-v/--verbose", "Increase verbosity level"}.counter()]] int verbose = 1;

    struct[[= cli::command{"Commit changes."}]]
    {
      git &up;

      [[= cli::argument{"Commit message."}]] std::string message;

      int operator()() const
      {
        std::println("Committing with message: {}", message);
        return 0;
      }
    } commit{*this};

    struct[[= cli::command{"Push changes."}]]
    {
      git &up;
      
      [[= cli::option{"-r/--remote", "Remote name."}]] std::string remote = "origin";

      int operator()() const
      {
        std::println("Pushing to remote: {}", remote);
        return 0;
      }
    } push{*this};
  };
}
