#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <reflex/constant_span.hpp>
#include <reflex/core.hpp>
#include <reflex/heapless/vector.hpp>

#include <iostream>
#endif

REFLEX_EXPORT namespace reflex::cli
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

  [[= option{"-h/--help", "Print this message and exit."}.flag()]] constexpr bool help_option{
      false};

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

    consteval bool operator==(base_info const& other) const
    {
      return member == other.member;
    }
  };

  struct option_info : base_info<option_info, option>
  {
    struct _switches
    {
      constant_string s;
      constant_string l;
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

  enum class parsing_state
  {
    completed,
    completion_check,
    missing_option,
    missing_option_value,
    option_value_check,
    missing_argument,
    missing_command,
    unknown_option,
    unexpected_argument,
  };

  template <constant_span items> struct item_tracker
  {
    static constexpr auto N = items.size();
    std::array<bool, N>   _used{};

    using value_type = typename decltype(items)::value_type;

    constexpr void mark_used(std::size_t idx)
    {
      _used[idx] = true;
    }

    template <value_type value> constexpr void mark_used()
    {
      template for(constexpr auto ii : std::views::iota(0uz, N))
      {
        if constexpr(value == items.view()[ii])
        {
          _used[ii] = true;
          return;
        }
      }
    }

    constexpr bool is_used(std::size_t idx) const
    {
      return _used[idx];
    }

    template <value_type value> constexpr bool is_used()
    {
      template for(constexpr auto ii : std::views::iota(0uz, N))
      {
        if constexpr(value == items.view()[ii])
        {
          return _used[ii];
        }
      }
      std::unreachable();
    }

    static constexpr std::size_t size()
    {
      return N;
    }

    constexpr bool all_used() const
    {
      return std::ranges::all_of(_used, std::identity{});
    }

    auto unused([[maybe_unused]] auto fn) const
    {
      template for(constexpr auto ii : std::views::iota(0uz, N))
      {
        if(not _used[ii])
        {
          fn.template operator()<items[ii]>();
        }
      }
    }

    auto first_unused([[maybe_unused]] auto fn) const
    {
      template for(constexpr auto ii : std::views::iota(0uz, N))
      {
        if(not _used[ii])
        {
          fn.template operator()<items[ii]>();
          return;
        }
      }
    }

    auto last_used([[maybe_unused]] auto fn) const
    {
      // FIXME: this double loop is a workaround due to GCC bug...
      [[maybe_unused]] std::size_t last_used_idx = std::numeric_limits<std::size_t>::max();
      template for(constexpr auto ii : std::views::iota(0uz, N))
      {
        if(_used[ii])
        {
          last_used_idx = ii;
        }
      }
      template for(constexpr auto ii : std::views::iota(0uz, N))
      {
        if(ii == last_used_idx)
        {
          fn.template operator()<items[ii]>();
          return;
        }
      }
    }

    auto used([[maybe_unused]] auto fn) const
    {
      template for(constexpr auto ii : std::views::iota(0uz, N))
      {
        if(_used[ii])
        {
          fn(items.view()[ii]);
        }
      }
    }

    constexpr auto all([[maybe_unused]] auto fn) const
    {
      template for(constexpr auto ii : std::views::iota(0uz, N))
      {
        fn.template operator()<items[ii]>();
      }
    }
  };

  template <constant_span items> constexpr auto make_tracker()
  {
    return item_tracker<items>{};
  }

  template <typename Cmd> struct parse_trackers
  {
    static constexpr auto cmd_type = remove_cvref(^^Cmd);

    static constexpr auto _raw = raw_parse<cmd_type>();

    static constexpr auto args = argument_info::from_info_range(std::get<0>(_raw));
    static constexpr auto opts = option_info::from_info_range(std::get<1>(_raw));
    static constexpr auto cmds = command_info::from_info_range(std::get<2>(_raw));

    Cmd& root;

    decltype(make_tracker<constant_span(define_static_array(args))>()) args_track =
        make_tracker<constant_span(define_static_array(args))>();
    decltype(make_tracker<constant_span(define_static_array(opts))>()) opts_track =
        make_tracker<constant_span(define_static_array(opts))>();
    decltype(make_tracker<constant_span(define_static_array(cmds))>()) cmds_track =
        make_tracker<constant_span(define_static_array(cmds))>();

    struct _current
    {
      std::string_view view;
      std::string_view value_view;

      bool is_option() const
      {
        return not view.empty() and view[0] == '-';
      }

    } current{};

    void init_current(std::string_view v)
    {
      current.view       = v;
      current.value_view = {};
    }

    std::string_view program{};
    parsing_state    state = parsing_state::completed;
    std::size_t      index = 1;

    void usage() const
    {
      usage_of<cmd_type>(program);
    }
  };

  template <bool show_help = true, typename Cli>
  int process_cmdline(
      Cli&&            cli,
      std::string_view program,
      auto             it,
      auto             end,
      auto             state_handler,
      std::size_t      index = 1)
  {
    static constexpr auto cli_type = remove_cvref(^^Cli);
    parse_trackers<Cli>   trackers{cli};
    trackers.program = program;
    trackers.index   = index;

    bool        treat_as_argument = false;
    std::size_t current_pos_arg = 0;
    while(it != end)
    {
      trackers.init_current(*it);
      if(trackers.current.view.empty())
      {
        ++it;
        continue;
      }

      treat_as_argument = false;

      if(trackers.current.is_option())
      {
        // option lookup
        bool found = false;
        template for(constexpr auto o : trackers.opts)
        {
          auto [short_switch, long_switch] = o.switches;

          if((trackers.current.view == *short_switch)
             or (trackers.current.view == *long_switch)
             or
             // counters accepts repeated short switch (ie.: -vvv => counter = 3)
             (o.is_counter()
              and trackers.current.view.starts_with(short_switch)
              and (std::ranges::all_of(trackers.current.view | std::views::drop(2), [&](auto c) {
                    return c == short_switch[1];
                  }))))
          {
            if constexpr(o == ^^help_option)
            {
              if(show_help)
              {
                usage_of<cli_type>(program);
              }
              return 0;
            }
            else
            {
              constexpr auto type = o.type();
              using T             = [:type:];
              T& target           = cli.[:o.member:];
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
                    target.value() += std::ranges::count(trackers.current.view, short_switch[1]);
                  }
                  else
                  {
                    target += T(std::ranges::count(trackers.current.view, short_switch[1]));
                  }
                }
                else
                {
                  if(it != end)
                  {
                    ++it;
                  }
                  trackers.current.value_view = std::string_view{*it};
                  if(it == end)
                  {
                    trackers.state = parsing_state::missing_option_value;
                    state_handler(trackers);
                    return 1;
                  }
                  else
                  {
                    trackers.state = parsing_state::option_value_check;
                    state_handler(trackers);
                  }
                  ++trackers.index;

                  if constexpr(seq_c<T>)
                  {
                    target.push_back(
                        reflex::parse<typename T::value_type>(trackers.current.value_view));
                  }
                  else
                  {
                    target = reflex::parse<T>(trackers.current.value_view);
                  }
                }
              }
              trackers.opts_track.template mark_used<o>();
            }

            found = true;
            break;
          }
        }
        if(!found)
        {
          // If a positional argument is still expected, accept values prefixed with '-'
          // (for example negative numbers) as arguments instead of unknown options.
          if(current_pos_arg < trackers.args.size())
          {
            treat_as_argument = true;
          }
          else
          {
            trackers.state = parsing_state::unknown_option;
            state_handler(trackers);
            return 1;
          }
        }
      }

      if((not trackers.current.is_option()) or treat_as_argument)
      {
        template for(constexpr auto cmd : trackers.cmds)
        {
          if(trackers.current.view == cmd.display_name())
          {
            trackers.cmds_track.template mark_used<cmd>();
            if constexpr(requires { cli.template operator()<cmd.member>(); })
            {
              // groups may want initialization call
              cli.template operator()<cmd.member>();
            }
            else if constexpr(requires { cli(); })
            {
              cli();
            }
            if(it != end)
            {
              ++it;
            }
            trackers.state = parsing_state::completion_check;
            if(state_handler(trackers) != 0)
            {
              return 1;
            }
            ++trackers.index;
            return process_cmdline<show_help>(
                cli.[:cmd.member:], std::format("{} {}", program, trackers.current.view), it, end,
                                  state_handler, trackers.index);
          }
        }

        // assume argument
        bool found = false;
        template for(constexpr auto ii : std::views::iota(std::size_t(0), trackers.args.size()))
        {
          if(ii >= current_pos_arg)
          {
            ++current_pos_arg;
            constexpr auto arg  = argument_info{trackers.args[ii]};
            constexpr auto type = arg.type();
            using T             = [:type:];
            T& target           = cli.[:arg.member:];
            trackers.args_track.template mark_used<arg>();

            if constexpr(seq_c<T>)
            {
              const_assert(
                  ii == trackers.args.size() - 1, "repeated arguments must be last",
                  arg.source_location());
              do
              {
                // consume all remaining arguments
                auto view = std::string_view(*it);
                target.push_back(reflex::parse<typename T::value_type>(view));
                ++trackers.index;
              } while(++it != end);
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
          trackers.state = parsing_state::unexpected_argument;
          state_handler(trackers);
          return 1;
        }
      }
      if(it != end)
      {
        ++it;
        ++trackers.index;
      }
    }

    constexpr auto arg_count = trackers.args.size();
    if(current_pos_arg < arg_count)
    {
      template for(constexpr auto ii : std::views::iota(std::size_t(0), trackers.args.size()))
      {
        if(ii >= current_pos_arg)
        {
          constexpr auto arg  = argument_info{trackers.args[ii]};
          constexpr auto type = arg.type();
          if constexpr(meta::is_template_instance_of(type, ^^std::optional))
          {
            ++current_pos_arg;
          }
          else
          {
            trackers.state = parsing_state::missing_argument;
            state_handler(trackers);
            return 1;
          }
        }
      }
    }

    if(treat_as_argument and trackers.current.is_option())
    {
      trackers.state = parsing_state::unknown_option;
      state_handler(trackers);
      return 1;
    }

    if constexpr(requires { cli(); })
    {
      trackers.state = parsing_state::completed;
      return state_handler(trackers);
    }
    else
    {
      trackers.state = parsing_state::missing_command;
      state_handler(trackers);
      return 1;
    }
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