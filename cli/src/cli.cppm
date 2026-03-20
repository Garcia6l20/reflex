export module reflex.cli;

export import reflex.core;

import std;

export import :base;
export import :completion;

namespace reflex::cli::detail
{
template <meta::info I>
std::optional<int>
    process_cmdline(std::string_view program, auto it, auto end, auto get_item, auto on_command)
{
  static constexpr auto [args, opts, s_cmds] = parse<I>();

  std::size_t current_pos_arg = 0;
  while(it != end)
  {
    auto view = std::string_view{*it};
    if(view[0] == '-')
    {
      // option lookup
      bool found = false;
      template for(constexpr auto o : opts)
      {
        auto [short_switch, long_switch] = o.switches;

        if((view == short_switch)
           or (view == long_switch)
           or
           // counters accepts repeated short switch (ie.: -vvv => counter = 3)
           (o.is_counter()
            and view.starts_with(short_switch)
            and (std::ranges::all_of(
                view | std::views::drop(2), [&](auto c) { return c == short_switch[1]; }))))
        {
          if constexpr(o == ^^help_option)
          {
            usage_of<I>(program);
            return 0;
          }
          else
          {
            constexpr auto type = o.type();
            using T             = [:type:];
            T& target           = get_item.template operator()<o.member, T>(o.name());
            if constexpr(type == ^^bool)
            {
              target = true;
            }
            else
            {
              if constexpr(o.is_counter())
              {
                if constexpr(meta::is_template_instance_of(type, ^^std::optional))
                {
                  if(not target.has_value())
                  {
                    target.emplace();
                  }
                  target.value() += std::ranges::count(view, short_switch[1]);
                }
                else
                {
                  target += std::ranges::count(view, short_switch[1]);
                }
              }
              else
              {
                it              = std::next(it);
                auto value_view = std::string_view{*it};
                target          = reflex::parse<T>(value_view);
              }
            }
          }

          found = true;
          break;
        }
      }
      if(!found)
      {
        std::println(std::cerr, "unknown option: {}", view);
        std::println(std::cerr);
        usage_of<I>(program);
        return 1;
      }
    }
    else
    {
      template for(constexpr auto cmd : s_cmds)
      {
        if(view == cmd.display_name())
        {
          return on_command.template operator()<cmd.member>(it, end);
        }
      }

      // assume argument
      auto value_view = view;
      bool found      = false;
      template for(constexpr auto ii : std::views::iota(std::size_t(0), args.size()))
      {
        if(ii >= current_pos_arg)
        {
          ++current_pos_arg;
          constexpr auto arg  = argument_info{args[ii]};
          constexpr auto type = arg.type();
          using T             = [:type:];
          T& target           = get_item.template operator()<arg.member, T>(arg.name());

          if constexpr(seq_c<T>)
          {
            const_assert(
                ii == args.size() - 1, "repeated arguments must be last", arg.source_location());
            auto prev = it;
            do
            {
              // consume all remaining arguments
              auto view = std::string_view(*it);
              target.push_back(reflex::parse<typename T::value_type>(view));
              prev = it;
            } while(++it != end);
            it = prev;
          }
          else
          {
            auto view = std::string_view(*it);
            target    = reflex::parse<T>(view);
          }
          found = true;
          break;
        }
      }
      if(!found)
      {
        std::println(std::cerr, "unexpected argument: {}", view);
        std::println(std::cerr);
        usage_of<I>(program);
        return 1;
      }
    }
    it = std::next(it);
  }

  constexpr auto arg_count = args.size();
  if(current_pos_arg < arg_count)
  {
    template for(constexpr auto ii : std::views::iota(std::size_t(0), args.size()))
    {
      if(ii >= current_pos_arg)
      {
        constexpr auto arg  = argument_info{args[ii]};
        constexpr auto type = arg.type();
        if constexpr(meta::is_template_instance_of(type, ^^std::optional))
        {
          ++current_pos_arg;
        }
        else
        {
          std::print(std::cerr, "missing required argument: ");
          bool first = true;
          template for(constexpr auto jj : std::views::iota(std::size_t(ii), args.size()))
          {
            if(jj >= current_pos_arg)
            {
              if(!first)
              {
                std::print(std::cerr, ", ");
              }
              constexpr auto name = arg.display_name();
              std::print(std::cerr, "{}", name);
              first = false;
            }
          }
          std::println(std::cerr);
          std::println(std::cerr);
          usage_of<I>(program);
          return 1;
        }
      }
    }
  }

  // if constexpr(not sub_commands.empty())
  // {
  //   template for(constexpr auto [sub_command, mem] : sub_commands)
  //   {
  //     if constexpr(has_flag<I, mem, ^^_default>())
  //     {
  //       return on_command.template operator()<mem>(it, end);
  //     }
  //   }
  // }

  return std::nullopt;
}
} // namespace reflex::cli::detail

export namespace reflex::cli
{
int run(auto cli, std::string_view program, auto it, auto end)
{
  constexpr auto cli_type = type_of(^^cli);
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

int run(auto cli, int argc, const char** argv)
{
  auto program = std::string_view{argv[0]};
  if(auto pos = program.find_last_of("/"); pos != std::string_view::npos)
  {
    program.remove_prefix(pos + 1);
  }
  return run(cli, program, argv + 1, argv + argc);
}

template <std::ranges::range R = std::initializer_list<std::string_view>>
int run(auto cli, R&& args)
{
  auto argv = args
            | std::views::transform([](std::string_view arg) { return arg.data(); })
            | std::ranges::to<std::vector>();
  return cli::run(cli, argv.size(), argv.data());
}

} // namespace reflex::cli
