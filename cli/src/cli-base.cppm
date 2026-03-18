export module reflex.cli:base;

import reflex.core;

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
    std::string_view      view  = switches.view();
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

consteval std::string_view display_name_of(std::meta::info mem)
{
  return constant_string{caseconv::to_kebab_case(identifier_of(mem))};
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
      max_id_size = std::max(max_id_size, display_name_of(mem).size());
    }
    template for(constexpr auto mem : arguments)
    {
      constexpr auto        arg  = meta::annotation_value_of_with<argument>(mem);
      static constexpr auto help = arg.help.view();
      std::println("  {:{}} {}", display_name_of(mem), max_id_size, help);
    }

    std::println();
  }

  if constexpr(not sub_commands.empty())
  {
    std::println("COMMANDS:");
    std::size_t max_id_size = min_id_size;
    template for(constexpr auto mem : sub_commands)
    {
      max_id_size = std::max(max_id_size, display_name_of(mem).size());
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
      std::println("  {:{}} {}", display_name_of(mem), max_id_size, help);
    }
  }

  std::println();
}
} // namespace reflex::cli::detail