#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <filesystem>
#include <optional>
#endif

#include <reflex/cli/base.hpp>
#include <reflex/cli/completion.hpp>

REFLEX_EXPORT namespace reflex::cli
{
  namespace detail
  {
  template <typename Cli, typename Invoker = decltype(detail::default_invoker)>
  int process(
      Cli&& cli, std::string_view executable, auto it, auto end,
      Invoker invoker = detail::default_invoker)
  {
    auto command = std::filesystem::path{executable}.filename().string();
    return detail::process_cmdline(
        std::forward<Cli>(cli), command, executable, it, end, [](auto const& trackers) {
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
          else if(state == parsing_state::missing_option_value)
          {
            std::println(std::cerr, "missing value for option: {}", view);
            std::println(std::cerr);
          }
          else if(state == parsing_state::invalid_option_value)
          {
            std::println(
                std::cerr, "invalid value for option {}: {} ({})", view,
                trackers.current.value_view, std::generic_category().message(int(trackers.current.parse_error)));
            std::println(std::cerr);
          }
          else if(state == parsing_state::invalid_argument_value)
          {
            std::println(
                std::cerr, "invalid argument value: {} ({})", view,
                std::generic_category().message(int(trackers.current.parse_error)));
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
                           { trackers.invoke() } -> std::convertible_to<int>;
                         })
            {
              return trackers.invoke();
            }
            else if constexpr(requires { trackers.invoke(); })
            {
              trackers.invoke();
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
        }, 1, invoker);
  }

  /** @brief answer the shell instead of running the command
   *
   * Empty unless the completion protocol is active, in which case the returned
   * value is what the program must exit with.
   */
  template <configuration config> std::optional<int> try_complete(auto& cmd)
  {
    if constexpr(config.completion.enabled)
    {
      if(const auto complete_env = std::getenv("_REFLEX_COMPLETE"); complete_env != nullptr)
      {
        std::string_view const complete{complete_env};
        if(not complete.empty() and complete.ends_with("complete"))
        {
          return detail::do_complete<config>(cmd);
        }
      }
    }
    return std::nullopt;
  }
  } // namespace detail

  template <typename Cli, configuration config = {}>
  int run(Cli && cli, std::string_view executable, auto it, auto end)
  {
    if(const auto rc = detail::try_complete<config>(cli))
    {
      return *rc;
    }

    return detail::process(cli, executable, it, end);
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

  /** @brief run a plain function as a command
   *
   * The function's parameters are the command's arguments and options, carrying
   * their own annotations. Nothing is written at the call site beyond the
   * reflection of the function.
   */
  template <std::meta::info Fn, configuration config = {}>
  int run(std::string_view executable, auto it, auto end)
  {
    detail::command_args<Fn> args{};

    if(const auto rc = detail::try_complete<config>(args))
    {
      return *rc;
    }

    return detail::process(args, executable, it, end, detail::function_invoker<Fn>);
  }

  template <std::meta::info Fn, configuration config = {}> int run(int argc, const char** argv)
  {
    return cli::run<Fn, config>(std::string_view{argv[0]}, argv + 1, argv + argc);
  }

  template <
      std::meta::info Fn, configuration config = {},
      std::ranges::range R = std::initializer_list<std::string_view>>
  int run(R && args)
  {
    auto argv = args
              | std::views::transform([](std::string_view arg) { return arg.data(); })
              | std::ranges::to<std::vector>();
    return cli::run<Fn, config>(int(argv.size()), argv.data());
  }

} // namespace reflex::cli
