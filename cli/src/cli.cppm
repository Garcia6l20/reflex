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

struct complete
{
  std::meta::info completer;
};

struct command
{
  constant_string help;
};
} // namespace reflex::cli

namespace reflex::cli::detail
{
[[= option{"-h/--help", "Print this message and exit."}.flag()]] constexpr bool help_option{false};

template <std::meta::info I> constexpr auto parse()
{
  std::vector<std::meta::info> arguments;
  std::vector<std::meta::info> options;
  std::vector<std::meta::info> sub_commands;

  options.push_back(^^help_option);

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
        continue;
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
  return std::tuple{
      define_static_array(arguments), define_static_array(options),
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

  constexpr auto help = [&] consteval {
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
      constexpr auto cmd = [&mem] consteval {
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
            and (std::ranges::all_of(
                view | std::views::drop(2), [&](auto c) { return c == short_switch[1]; }))))
        {
          if constexpr(mem == ^^help_option)
          {
            usage_of<I>(program);
            return 0;
          }
          else
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

// === Dynamic completion ===
//
// Protocol (environment variables read by the binary):
//   _REFLEX_COMPLETE=xxx_complete  - activates completion mode
//   _REFLEX_COMP_LINE="git pu"     - full command line typed so far
//   _REFLEX_COMP_POINT=6           - cursor position (unhandled for now)
//

enum class completion_type
{
  plain, // simple completion with no description
  dir,   // complete with directories
  file   // complete with files
};

struct completion
{
  completion_type  type;
  std::string_view value;
  std::string_view description;

  void print() const
  {
    std::println("{}\n{}\n{}", type, value, description);
  }
};

using word_vector       = std::inplace_vector<std::string_view, 32>;
using completion_vector = std::inplace_vector<completion, 16>;

template <meta::info I> auto complete_for(word_vector words, std::string_view current)
{
  completion_vector completions{};

  static constexpr auto [arguments, options, sub_commands] = parse<I>();

  // If there are still words before the cursor that are not the current one,
  // check whether the first of them names a sub-command we should recurse into.
  if(not words.empty())
  {
    template for(constexpr auto mem : sub_commands)
    {
      auto it = std::ranges::find(words, identifier_of(mem));
      if(it != words.end())
      {
        constexpr auto sub_type = type_of(mem);
        return complete_for<sub_type>(word_vector{it + 1, words.end()}, current);
      }
    }
  }
  template for(constexpr auto mem : sub_commands)
  {
    if(current == identifier_of(mem))
    {
      constexpr auto sub_type = type_of(mem);
      return complete_for<sub_type>(words, {});
    }
  }

  // Emit matching sub-command names.
  template for(constexpr auto mem : sub_commands)
  {
    constexpr auto name = identifier_of(mem);
    if(current.empty() or name.starts_with(current))
    {
      constexpr auto cmd = meta::annotation_value_of_with<command>(type_of(mem));
      completions.push_back(
          {.type = completion_type::plain, .value = name, .description = cmd.help.view()});
    }
  }
  if(not current.empty() and not completions.empty())
  {
    return completions;
  }

  if(current.starts_with('-'))
  {
    // Emit matching option switches.
    template for(constexpr auto mem : options)
    {
      constexpr auto opt       = meta::annotation_value_of_with<option>(mem);
      auto [short_sw, long_sw] = opt.split_switches();
      if(std::ranges::find_if(words, [&](auto w) { return w == long_sw or w == short_sw; })
         != words.end())
      {
        continue; // already present in command line, don't offer again
      }

      if(current == long_sw)
      {
        return completion_vector{
            {.type = completion_type::plain, .value = long_sw, .description = opt.help.view()}
        };
      }
      else if(current == short_sw)
      {
        return completion_vector{
            {.type = completion_type::plain, .value = short_sw, .description = opt.help.view()}
        };
      }

      constexpr auto descr = opt.help.view();
      if(long_sw.starts_with(current))
      {
        completions.push_back(
            {.type = completion_type::plain, .value = long_sw, .description = descr});
      }
      if(short_sw.starts_with(current))
      {
        completions.push_back(
            {.type = completion_type::plain, .value = short_sw, .description = descr});
      }
    }
  }
  return completions;
}

std::optional<std::string_view> next_word(std::string_view& line)
{
  line = ltrim(line);
  if(line.empty())
  {
    return std::nullopt;
  }
  auto pos = line.find(' ');
  if(pos == std::string_view::npos)
  {
    auto word = line;
    line      = {};
    return word;
  }
  else
  {
    auto word = line.substr(0, pos);
    line      = line.substr(pos);
    return word;
  }
}

// Entry-point called from run() when _REFLEX_COMPLETE is set to xxx_complete.
template <meta::info I> int do_complete()
{
  std::string_view comp_line = [] {
    if(const auto env = std::getenv("_REFLEX_COMP_LINE"); env != nullptr)
      return std::string_view{env};
    return std::string_view{};
  }();

  if(comp_line.empty())
  {
    return 1;
  }

  const std::string_view comp_point_env = [] {
    if(const auto env = std::getenv("_REFLEX_COMP_POINT"); env != nullptr)
      return std::string_view{env};
    return std::string_view{};
  }();

  // TODO comp point not handled !
  std::size_t comp_point = 1;
  if(not comp_point_env.empty())
  {
    auto res = std::from_chars(
        comp_point_env.data(), comp_point_env.data() + comp_point_env.size(), comp_point);
    if(res.ec != std::errc{})
    {
      return 1; // skip
    }
    if(comp_point < 1)
    {
      return 1; // invalid
    }
  }

  next_word(comp_line); // drop command name

  word_vector      words;
  std::string_view current;
  while(true)
  {
    if(auto word = next_word(comp_line); word.has_value())
    {
      if(--comp_point == 0)
      {
        current = word.value();
        break;
      }
      words.push_back(word.value());
    }
    else
    {
      break;
    }
  }

  const auto completions = complete_for<I>(words, current);
  for(auto const& comp : completions)
  {
    comp.print();
  }
  return 0;
}
void emit_zsh_source(std::string_view program);
void emit_bash_source(std::string_view program);
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

int run(auto cli, std::initializer_list<std::string_view> args)
{
  auto argv = args
            | std::views::transform([](std::string_view arg) { return arg.data(); })
            | std::ranges::to<std::vector>();
  return cli::run(cli, argv.size(), argv.data());
}

} // namespace reflex::cli
