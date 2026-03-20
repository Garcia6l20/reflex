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
namespace detail
{

[[= option{"-h/--help", "Print this message and exit."}.flag()]] constexpr bool help_option{false};

template <typename Self, typename AnnotationT> struct base_info
{
  std::meta::info member;

  using annotation_type = AnnotationT;
  annotation_type const& annotation;

  static consteval annotation_type const& __get_annotation(std::meta::info mem)
  {
    static constexpr annotation_type empty;
    try
    {
      return meta::annotation_value_of_with<annotation_type>(mem);
    }
    catch(std::meta::exception const&)
    {
      try
      {
        return meta::annotation_value_of_with<annotation_type>(type_of(mem));
      }
      catch(std::meta::exception const&)
      {
        return empty;
      }
    }
  }

  consteval base_info(std::meta::info mem) : member{mem}, annotation{__get_annotation(mem)}
  {}

  consteval std::string_view name() const
  {
    return {identifier_of(member)};
  }
  consteval std::string_view display_name() const
  {
    return constant_string{caseconv::to_kebab_case(identifier_of(member))};
  }
  consteval decltype(auto) help() const
  {
    return annotation.help;
  }
  consteval std::meta::info type() const
  {
    return type_of(member);
  }
  consteval std::source_location source_location() const
  {
    return source_location_of(member);
  }

  static consteval auto from_info_range(std::span<std::meta::info const> infos)
  {
    return infos | std::views::transform([](auto mem) { return Self{mem}; });
  }

  consteval bool operator==(std::meta::info const& mem) const
  {
    return member == mem;
  }
};

struct option_info : base_info<option_info, option>
{
  struct _switches
  {
    std::string_view s;
    std::string_view l;
  } switches;

  consteval option_info(std::meta::info mem)
      : base_info{mem}, switches{std::make_from_tuple<_switches>(annotation.split_switches())}
  {}

  consteval bool is_counter() const
  {
    return annotation.is_counter;
  }
};

struct argument_info : base_info<argument_info, argument>
{
  consteval argument_info(std::meta::info mem) : base_info{mem}
  {}
};

struct command_info : base_info<command_info, command>
{
  consteval command_info(std::meta::info mem) : base_info{mem}
  {}
};

template <std::meta::info I> constexpr auto raw_parse()
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

template <std::meta::info I> constexpr auto parse()
{
  static constexpr auto [arguments, options, sub_commands] = raw_parse<I>();
  static constexpr auto args   = argument_info::from_info_range(arguments);
  static constexpr auto opts   = option_info::from_info_range(options);
  static constexpr auto s_cmds = command_info::from_info_range(sub_commands);
  return std::tuple{args, opts, s_cmds};
}

consteval std::string_view display_name_of(std::meta::info mem)
{
  return constant_string{caseconv::to_kebab_case(identifier_of(mem))};
}

template <std::meta::info I> void usage_of(std::string_view program)
{
  static constexpr auto command              = command_info{I};
  static constexpr auto [args, opts, s_cmds] = parse<I>();

  static constexpr std::size_t min_id_size = 16;

  if(auto pos = program.find_last_of("/"); pos != std::string_view::npos)
  {
    program.remove_prefix(pos + 1);
  }

  std::println("USAGE: {} [OPTIONS...] ARGUMENTS...", program);

  if constexpr(constexpr auto help = command.help(); not help.empty())
  {
    std::println();
    std::println("{}", help);
    std::println();
  }

  {
    std::println("OPTIONS:");

    std::size_t max_id_size = min_id_size;
    template for(constexpr auto opt : opts)
    {
      constexpr auto sz = opt.switches.s.size() + opt.switches.l.size() + 1;
      max_id_size       = std::max(max_id_size, sz);
    }
    template for(constexpr auto opt : opts)
    {
      constexpr auto [s, l] = opt.switches;
      auto switches_str     = std::format("{}/{}", s, l);
      std::println("  {:{}} {}", switches_str, max_id_size, opt.help());
    }

    std::println();
  }

  if constexpr(not args.empty())
  {
    std::println("ARGUMENTS:");
    std::size_t max_id_size = min_id_size;
    template for(constexpr auto a : args)
    {
      max_id_size = std::max(max_id_size, a.display_name().size());
    }
    template for(constexpr auto a : args)
    {
      std::println("  {:{}} {}", a.display_name(), max_id_size, a.help());
    }

    std::println();
  }

  if constexpr(not s_cmds.empty())
  {
    std::println("COMMANDS:");
    std::size_t max_id_size = min_id_size;
    template for(constexpr auto c : s_cmds)
    {
      max_id_size = std::max(max_id_size, c.display_name().size());
    }
    template for(constexpr auto c : s_cmds)
    {
      std::println("  {:{}} {}", c.display_name(), max_id_size, c.help());
    }
  }

  std::println();
}

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

bool tokenize(str_c auto const& line, std::output_iterator<std::string_view> auto&& out_tokens)
{
  auto view = std::string_view{line};
  view      = ltrim(view);
  while(not view.empty())
  {
    if(view[0] == '"')
    {
      // Quoted token
      auto end_quote = view.find('"', 1);
      if(end_quote == std::string_view::npos)
      {
        return false;
      }
      out_tokens = view.substr(1, end_quote - 1);
      view.remove_prefix(end_quote + 1);
      view = ltrim(view);
    }
    else
    {
      // Unquoted token
      auto next_space = view.find(' ');
      if(next_space == std::string_view::npos)
      {
        next_space = view.size();
      }
      out_tokens = view.substr(0, next_space);
      view.remove_prefix(next_space);
      view = ltrim(view);
    }
  }
  return true;
}
} // namespace detail
} // namespace reflex::cli