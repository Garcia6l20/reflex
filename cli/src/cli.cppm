export module reflex.cli;

export import reflex.core;

import std;

export import :base;
export import :completion;

export namespace reflex::cli
{
template <typename Cli> int run(Cli&& cli, std::string_view program, auto it, auto end)
{
  constexpr auto cli_type = decay(type_of(^^cli));
  {
    // Completion management
    if(const auto complete_env = std::getenv("_REFLEX_COMPLETE"); complete_env != nullptr)
    {
      std::string_view const complete{complete_env};
      if(not complete.empty())
      {
        if(complete.ends_with("complete"))
        {
          return detail::do_complete<cli_type>();
        }
        else if(complete == "bash_source")
        {
          detail::emit_bash_source(program);
          return 0;
        }
        else if(complete == "zsh_source")
        {
          detail::emit_zsh_source(program);
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

  const auto rc = detail::process_cmdline<cli_type>( //
      program,
      it,
      end,
      //
      // Get item
      //
      [&]<meta::info mem, typename T>(std::string_view name) -> std::reference_wrapper<T>
      { return std::ref(cli.[:mem:]); },
      //
      // On command
      //
      [&]<meta::info mem>(auto it, auto end)
      {
        constexpr auto type = type_of(mem);
        using T             = [:type:];
        auto view           = std::string_view(it != end ? *it : "");

        return cli::run(cli.[:mem:], std::format("{} {}", program, view), std::next(it), end);
      });
  if(rc.has_value())
  {
    return rc.value();
  }

  if constexpr(requires { cli.operator()(); })
  {
    return cli.operator()();
  }
  else
  {
    std::println(std::cerr, "Error: missing subcommand");
    return -1;
  }
}

template <typename Cli> int run(Cli&& cli, int argc, const char** argv)
{
  auto program = std::string_view{argv[0]};
  if(auto pos = program.find_last_of("/"); pos != std::string_view::npos)
  {
    program.remove_prefix(pos + 1);
  }
  return run(std::forward<Cli>(cli), program, argv + 1, argv + argc);
}

template <typename Cli, std::ranges::range R = std::initializer_list<std::string_view>>
int run(Cli&& cli, R&& args)
{
  auto argv = args
            | std::views::transform([](std::string_view arg) { return arg.data(); })
            | std::ranges::to<std::vector>();
  return cli::run(std::forward<Cli>(cli), argv.size(), argv.data());
}

} // namespace reflex::cli
