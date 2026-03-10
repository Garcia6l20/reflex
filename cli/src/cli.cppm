export module reflex.cli;

export import reflex.core;
import std;

export namespace reflex::cli
{
struct argument
{
  constant_string help;
};

struct option
{
  constant_string switches;
  constant_string help;

  bool is_flag    = false;
  bool is_counter = false;

  consteval option& flag()
  {
    is_flag = true;
    return *this;
  }

  consteval option& counter()
  {
    is_flag    = true;
    is_counter = true;
    return *this;
  }

  constexpr std::tuple<std::string_view, std::string_view> split_switches() const
  {
    static constexpr auto npos  = std::string_view::npos;
    std::string_view      view  = switches;
    auto                  slash = view.find('/');

    if(slash != npos)
    {
      if(view[1] == '-')
      {
        return {view.substr(0, slash), view.substr(slash + 1)};
      }
      else
      {
        return {view.substr(0, slash), view.substr(slash + 1)};
      }
    }
    else
    {
      // No slash
      if(view[1] == '-')
      {
        return {"", view};
      }
      else
      {
        return {view, ""};
      }
    }
  }
};

struct command
{
  constant_string help;
};
} // namespace reflex::cli

namespace reflex::cli::detail
{

template <std::meta::info I> constexpr auto parse()
{
  std::vector<std::meta::info> arguments;
  std::vector<std::meta::info> options;
  std::vector<std::meta::info> flags;
  std::vector<std::meta::info> sub_commands;

  template for(constexpr auto mem :
               define_static_array(nonstatic_data_members_of(I, meta::access_context::current())))
  {
    if constexpr(is_function(mem))
    {
      continue;
    }
    auto annotations = annotations_of(mem);
    if(annotations.empty())
    {
      // check if member is a sub-command
      auto type = type_of(mem);
      if(is_class_type(type) and meta::has_annotation(type, ^^command))
      {
        sub_commands.push_back(mem);
      }
      else
      {
        throw std::logic_error(
            std::string(display_string_of(mem))
            + ": must be annotated with cli::argument, cli::option, or cli::flag");
      }
    }
    else
    {
      for(auto a : annotations)
      {
        auto AT = decay(type_of(constant_of(a)));
        if(AT == ^^argument)
        {
          arguments.push_back(mem);
        }
        else if(AT == ^^option)
        {
          options.push_back(mem);
        }
        else if(AT == ^^command)
        {
          sub_commands.push_back(mem);
        }
      }
    }
  }
  return std::tuple{define_static_array(arguments),
                    define_static_array(options),
                    define_static_array(sub_commands)};
}

template <std::meta::info I> void usage_of(std::string_view program)
{
  static constexpr auto [arguments, options, sub_commands] = parse<I>();

  static constexpr std::size_t min_id_size = 16;

  if(auto pos = program.find_last_of("/"); pos != std::string_view::npos)
  {
    program.remove_prefix(pos + 1);
  }

  std::println("USAGE: {} [OPTIONS...] ARGUMENTS...", program);

  constexpr auto help = [&] consteval
  {
    try
    {
      return meta::annotation_value_of_with<command>(I).help.view();
    }
    catch(const std::meta::exception&)
    {
      return std::string_view{};
    }
  }();

  if(not help.empty())
  {
    std::println();
    std::println("{}", help);
    std::println();
  }

  {
    std::println("OPTIONS:");

    std::size_t max_id_size = min_id_size;
    template for(constexpr auto mem : options)
    {
      constexpr auto opt               = meta::annotation_value_of_with<option>(mem);
      auto [short_switch, long_switch] = opt.split_switches();
      max_id_size = std::max(max_id_size, short_switch.size() + long_switch.size() + 1);
    }
    std::println("  {:{}} {}", "-h/--help", max_id_size, "Print this message and exit.");
    template for(constexpr auto mem : options)
    {
      constexpr auto opt                 = meta::annotation_value_of_with<option>(mem);
      auto [short_switch, long_switch]   = opt.split_switches();
      auto                  switches_str = std::format("{}/{}", short_switch, long_switch);
      static constexpr auto help         = opt.help.view();
      std::println("  {:{}} {}", switches_str, max_id_size, help);
    }

    std::println();
  }

  if constexpr(not arguments.empty())
  {
    std::println("ARGUMENTS:");
    std::size_t max_id_size = min_id_size;
    template for(constexpr auto mem : arguments)
    {
      max_id_size = std::max(max_id_size, identifier_of(mem).size());
    }
    template for(constexpr auto mem : arguments)
    {
      constexpr auto        arg  = meta::annotation_value_of_with<argument>(mem);
      static constexpr auto help = arg.help.view();
      std::println("  {:{}} {}", identifier_of(mem), max_id_size, help);
    }

    std::println();
  }

  if constexpr(not sub_commands.empty())
  {
    std::println("COMMANDS:");
    std::size_t max_id_size = min_id_size;
    template for(constexpr auto mem : sub_commands)
    {
      max_id_size = std::max(max_id_size, identifier_of(mem).size());
    }
    template for(constexpr auto mem : sub_commands)
    {
      constexpr auto cmd = [&mem] consteval
      {
        try
        {
          return meta::annotation_value_of_with<command>(mem);
        }
        catch(const std::meta::exception&)
        {
          return meta::annotation_value_of_with<command>(type_of(mem));
        }
      }();
      static constexpr auto help = cmd.help.view();
      std::println("  {:{}} {}", identifier_of(mem), max_id_size, help);
    }
  }

  std::println();
}

template <meta::info I>
std::optional<int>
    process_cmdline(std::string_view program, auto it, auto end, auto get_item, auto on_command)
{
  static constexpr auto [arguments, options, sub_commands] = parse<I>();

  std::size_t current_pos_arg = 0;
  while(it != end)
  {
    auto view = std::string_view{*it};
    if(view[0] == '-')
    {
      // option lookup
      bool found = false;
      if(view == "-h" or view == "--help")
      {
        usage_of<I>(program);
        return 0;
      }
      template for(constexpr auto mem : options)
      {
        constexpr auto opt               = meta::annotation_value_of_with<option>(mem);
        auto [short_switch, long_switch] = opt.split_switches();

        if((view == short_switch)
           or (view == long_switch)
           or
           // counters accepts repeated short switch (ie.: -vvv => counter = 3)
           (opt.is_counter
            and view.starts_with(short_switch)
            and (std::ranges::all_of(view | std::views::drop(2),
                                     [&](auto c) { return c == short_switch[1]; }))))
        {
          constexpr auto type = type_of(mem);
          using T             = [:type:];
          T& target           = get_item.template operator()<mem, T>(identifier_of(mem));
          if constexpr(type == ^^bool)
          {
            target = true;
          }
          else
          {
            if constexpr(opt.is_counter)
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
      template for(constexpr auto mem : sub_commands)
      {
        if(view == identifier_of(mem))
        {
          return on_command.template operator()<mem>(it, end);
        }
      }

      // assume argument
      auto value_view = view;
      bool found      = false;
      template for(constexpr auto ii : std::views::iota(std::size_t(0), arguments.size()))
      {
        if(ii >= current_pos_arg)
        {
          ++current_pos_arg;
          constexpr auto mem  = arguments[ii];
          constexpr auto type = type_of(mem);
          using T             = [:type:];
          T& target           = get_item.template operator()<mem, T>(identifier_of(mem));
          auto                          view = std::string_view(*it);
          if constexpr(type == ^^bool)
          {
            target = view == "true";
          }
          else
          {
            target = reflex::parse<T>(view);
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

  constexpr auto arg_count = arguments.size();
  if(current_pos_arg < arg_count)
  {
    template for(constexpr auto ii : std::views::iota(std::size_t(0), arguments.size()))
    {
      if(ii >= current_pos_arg)
      {
        constexpr auto type = type_of(arguments[ii]);
        if constexpr(meta::is_template_instance_of(type, ^^std::optional))
        {
          ++current_pos_arg;
        }
        else
        {
          std::print(std::cerr, "missing required argument: ");
          bool first = true;
          template for(constexpr auto jj : std::views::iota(std::size_t(ii), arguments.size()))
          {
            if(jj >= current_pos_arg)
            {
              if(!first)
              {
                std::print(std::cerr, ", ");
              }
              constexpr auto mem = arguments[jj];
              std::print(std::cerr, "{}", identifier_of(mem));
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
  const auto rc = detail::process_cmdline<type_of(^^cli)>( //
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

int run(auto cli, std::initializer_list<std::string_view> args)
{
  auto argv = args
            | std::views::transform([](std::string_view arg) { return arg.data(); })
            | std::ranges::to<std::vector>();
  return cli::run(cli, argv.size(), argv.data());
}

} // namespace reflex::cli
