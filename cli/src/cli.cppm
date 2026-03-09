export module reflex.cli;

export import reflex.core;

import std;

export namespace reflex::cli
{

class command;

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
    auto                  slash = switches.view().find('/');

    if(slash != npos)
    {
      if(switches.view()[1] == '-')
      {
        return {switches.view().substr(0, slash), switches.view().substr(slash + 1)};
      }
      else
      {
        return {switches.view().substr(0, slash), switches.view().substr(slash + 1)};
      }
    }
    else
    {
      // No slash
      if(switches.view()[1] == '-')
      {
        return {"", switches.view()};
      }
      else
      {
        return {switches.view(), ""};
      }
    }
  }
};

struct command
{
  constant_string help;
};

namespace detail
{
namespace _flags
{
struct count
{
};
struct default_
{
};
} // namespace _flags

constexpr _flags::count    _count;
constexpr _flags::default_ _default;

template <meta::info I, meta::info M> consteval auto flags_of()
{
  return annotations_of(M) //
       | std::views::filter(
             [&](auto A)
             {
               auto AT     = type_of(A);
               auto parent = parent_of(AT);
               return parent == ^^_flags;
             })                                    //
       | std::views::transform(std::meta::type_of) //
       | std::views::transform(std::meta::remove_const);
}

template <meta::info I, meta::info M, meta::info FLG> consteval bool has_flag()
{
  constexpr auto F = remove_const(type_of(FLG));
  return std::ranges::contains(flags_of<I, M>(), F);
}

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
      throw std::logic_error(std::string(display_string_of(mem))
                             + ": must be annotated with cli::argument, cli::option, or cli::flag");
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
    std::println("{}\n", help);
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
      constexpr auto flags = define_static_array(flags_of<I, mem>());
      if constexpr(flags.size())
      {
        using namespace std::string_view_literals;
        using std::views::join_with;
        using std::views::transform;
        constexpr auto ids = define_static_array( //
            flags                                 //
            | transform(std::meta::identifier_of) //
            | transform([](auto id) { return constant_string{id}; }));
        std::println("      flags: {}",
                     ids
                         | transform([](auto const& s) { return *s; }) //
                         | join_with(", "sv)                           //
                         | std::ranges::to<std::string>());
      }
    }
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
  }

  if constexpr(not sub_commands.empty())
  {
    std::println("SUBCOMMANDS:");
    std::size_t max_id_size = min_id_size;
    template for(constexpr auto mem : sub_commands)
    {
      max_id_size = std::max(max_id_size, identifier_of(mem).size());
    }
    template for(constexpr auto mem : sub_commands)
    {
      constexpr auto        cmd  = meta::annotation_value_of_with<command>(mem);
      static constexpr auto help = cmd.help.view();
      std::println("  {:{}} {}", identifier_of(mem), max_id_size, help);
    }
  }
}

template <int N = -1>
consteval auto call_args_to_tuple_type(std::meta::info type) -> std::meta::info
{
  constexpr auto remove_cvref = [](std::meta::info r) consteval
  {
    return substitute(^^std::remove_cvref_t,
                      {
                          r});
  };
  if constexpr(N < 0)
  {
    return substitute(^^std::tuple,
                      parameters_of(type)
                          | std::views::transform(std::meta::type_of)
                          | std::views::transform(remove_cvref)
                          | std::ranges::to<std::vector>());
  }
  else
  {
    return substitute(^^std::tuple,
                      parameters_of(type)
                          | std::views::take(N)
                          | std::views::transform(std::meta::type_of)
                          | std::views::transform(remove_cvref)
                          | std::ranges::to<std::vector>());
  }
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
        std::println("unknown option: {}", view);
        std::println();
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
        std::println("unexpected argument: {}", view);
        std::println();
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
          std::print("missing required argument: ");
          bool first = true;
          template for(constexpr auto jj : std::views::iota(std::size_t(ii), arguments.size()))
          {
            if(jj >= current_pos_arg)
            {
              if(!first)
              {
                std::print(", ");
              }
              constexpr auto mem = arguments[jj];
              std::print("{}", identifier_of(mem));
              first = false;
            }
          }
          std::println();
          std::println();
          usage_of<I>(program);
          return 1;
        }
      }
    }
  }

  if constexpr(not sub_commands.empty())
  {
    template for(constexpr auto [sub_command, mem] : sub_commands)
    {
      if constexpr(has_flag<I, mem, ^^_default>())
      {
        return on_command.template operator()<mem>(it, end);
      }
    }
  }

  return std::nullopt;
}
} // namespace detail

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

        return cli.[:mem:].run(std::format("{} {}", program, view), std::next(it), end);
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

} // namespace reflex::cli
