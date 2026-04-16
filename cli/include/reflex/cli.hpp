#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <filesystem>
#endif

#include <reflex/cli/base.hpp>
#include <reflex/cli/completion.hpp>

REFLEX_EXPORT namespace reflex::cli
{
  namespace detail
  {
  template <typename Cli> int process(Cli&& cli, std::string_view program, auto it, auto end)
  {
    return detail::process_cmdline(
        std::forward<Cli>(cli), program, it, end, [](auto const& trackers) {
          const auto state = trackers.state;
          const auto view  = trackers.current.view;

          using namespace detail;

          if(state == parsing_state::unknown_option)
          {
            std::println(std::cerr, "unknown option: {}", view);
            std::println(std::cerr);
          }
          else if(state == parsing_state::unexpected_argument)
          {
            std::println(std::cerr, "unexpected argument: {}", view);
            std::println(std::cerr);
          }
          else if(state == parsing_state::missing_argument)
          {
            std::println(std::cerr, "missing required argument: {}", view);
            std::println(std::cerr);
          }
          else if(state == parsing_state::invalid_option_value)
          {
            std::println(
                std::cerr, "invalid value for option {}: {} ({})", view,
                trackers.current.value_view, trackers.current.parse_error.message());
            std::println(std::cerr);
          }
          else if(state == parsing_state::invalid_argument_value)
          {
            std::println(
                std::cerr, "invalid argument value: {} ({})", view,
                trackers.current.parse_error.message());
            std::println(std::cerr);
          }
          else if(state == detail::parsing_state::missing_command)
          {
            std::println(std::cerr, "no command to execute");
            std::println(std::cerr);
          }
          else if(state == parsing_state::completed)
          {
            // actual command execution
            if constexpr(requires {
                           { trackers.root() } -> std::convertible_to<int>;
                         })
            {
              return trackers.root();
            }
            else if constexpr(requires { trackers.root(); })
            {
              trackers.root();
              return 0;
            }
            else
            {
              std::println(std::cerr, "no command to execute");
              std::println(std::cerr);
            }
          }
          else if(
              (state == parsing_state::option_value_check)
              or (state == parsing_state::completion_check))
          {
            return 0;
          }
          trackers.usage();
          return 1;
        });
  }
  } // namespace detail

  template <typename Cli, configuration config = {}>
  int run(Cli && cli, std::string_view executable, auto it, auto end)
  {
    // constexpr auto cli_type = decay(type_of(^^cli));
    if constexpr(config.completion.enabled)
    {
      // Completion management
      if(const auto complete_env = std::getenv("_REFLEX_COMPLETE"); complete_env != nullptr)
      {
        std::string_view const complete{complete_env};
        if(not complete.empty())
        {
          if(complete.ends_with("complete"))
          {
            return detail::do_complete<config>(cli);
          }
          else if(complete == "bash_source")
          {
            detail::emit_bash_source(executable);
            return 0;
          }
          else if(complete == "zsh_source")
          {
            detail::emit_zsh_source(executable);
            return 0;
          }
          else
          {
            std::println(std::cerr, "invalid value for _REFLEX_COMPLETE: {}", complete_env);
            return 1;
          }
        }
      }
    }

    auto program = std::filesystem::path{executable}.filename().string();
    return detail::process(cli, program, it, end);
  }

  template <typename Cli, configuration config = {}>
  int run(Cli && cli, int argc, const char** argv)
  {
    return run<Cli, config>(
        std::forward<Cli>(cli), std::string_view{argv[0]}, argv + 1, argv + argc);
  }

  template <typename Cli, configuration config = {}> int run(int argc, const char** argv)
  {
    return run<Cli, config>(Cli{}, argc, argv);
  }

  template <
      typename Cli, configuration config = {},
      std::ranges::range R = std::initializer_list<std::string_view>>
  int run(Cli && cli, R && args)
  {
    auto argv = args
              | std::views::transform([](std::string_view arg) { return arg.data(); })
              | std::ranges::to<std::vector>();
    return cli::run<Cli, config>(std::forward<Cli>(cli), int(argv.size()), argv.data());
  }

} // namespace reflex::cli
