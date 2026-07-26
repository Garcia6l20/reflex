#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <reflex/constant.hpp>
#include <reflex/core.hpp>
#include <reflex/heapless/vector.hpp>

#include <iostream>
#endif

REFLEX_EXPORT namespace reflex::cli
{
  enum class[[= derive(Format)]] completion_type
  {
    plain, // simple completion with no description
    dir,   // complete with directories
    file   // complete with files
  };

  template <typename Value, typename Description> struct completion_item
  {
    using value_type       = Value;
    using description_type = Description;

    completion_type  type = completion_type::plain;
    value_type       value;
    description_type description;

    void print() const
    {
      std::print("{}\n{}\n{}\n", type, value, description);
    }
  };

  struct completion_config
  {
    bool            enabled           = true;
    std::meta::info word_vector       = ^^std::vector<std::string_view>;
    std::meta::info completion_vector = ^^std::vector<completion_item<std::string, std::string>>;
  };

  struct configuration
  {
    completion_config completion{};
  };

  template <configuration config = {}>
  using word_vector = typename[:config.completion.word_vector:];

  template <configuration config = {}>
  using completion_vector = typename[:config.completion.completion_vector:];

  template <configuration config = {}>
  using completion = typename completion_vector<config>::value_type;

  struct argument
  {
    reflex::constant_string help = "";
  };

  struct option
  {
    reflex::constant_string switches = "";
    reflex::constant_string help     = "";

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
    reflex::constant_string help = "";
  };

  namespace detail
  {

  extern "C++" int install_completion(std::string_view executable, std::string_view shell);
  extern "C++" int emit_completion(std::string_view executable, std::string_view shell);

  [[= option{"--help", "Print this message and exit."}.flag()]] constexpr bool help_option{false};

  [[= option{"--install-completion", "Install shell completion."}
          .flag()]] constexpr bool install_completion_option{false};

  [[= option{"--show-completion", "Show shell completion."}
          .flag()]] constexpr bool show_completion_option{false};

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

    consteval bool is_flag() const
    {
      return annotation.is_flag;
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

  /** @brief refuse @p subject of command @p command, pointing at @p loc
   *
   * The messages are assembled rather than formatted because std::format is not
   * available in a constant expression under libstdc++.
   */
  consteval void refuse_command(
      std::string_view command, std::string_view subject, std::string_view reason,
      std::source_location loc)
  {
    std::string message{command};
    message += ": ";
    message += subject;
    message += ' ';
    message += reason;
    const_assert(false, message, loc);
  }

  /** @brief one data member per parameter of @p Fn, carrying its annotations
   *
   * A command is driven off an aggregate: the parser walks the members and
   * writes parsed values into them. Describing a function's parameters as
   * members lets that same walk read a function without knowing it is one.
   *
   * Every function command goes through here, so the refusals live here too.
   */
  template <std::meta::info Fn> consteval auto command_member_specs()
      -> std::vector<std::meta::info>
  {
    const auto name        = std::meta::identifier_of(Fn);
    const auto return_type = std::meta::return_type_of(Fn);

    if(not is_void_type(return_type) and not is_convertible_type(return_type, ^^int))
    {
      refuse_command(
          name, "returns", "something that is neither void nor convertible to int",
          source_location_of(Fn));
    }

    std::vector<std::meta::info> members;
    std::size_t                  position = 0;
    for(auto param : std::meta::parameters_of(Fn))
    {
      ++position;
      const auto where = source_location_of(param);

      if(not has_identifier(param))
      {
        refuse_command(name, "an unnamed parameter", "has no member to fill", where);
        continue;
      }

      const auto param_name = std::meta::identifier_of(param);
      const auto type       = std::meta::type_of(param);

      std::string subject{"parameter '"};
      subject += param_name;
      subject += '\'';

      // data_member_spec carries no initializer, so a declared default would be
      // silently replaced by value-initialization. std::optional<T> says the
      // same thing and the parser already treats it as optional.
      if(has_default_argument(param))
      {
        refuse_command(
            name, subject,
            "has a default argument, which a command cannot carry, "
            "use std::optional<T> for an optional argument",
            where);
      }

      // A reference member makes the aggregate non default constructible, and a
      // command line has nothing to bind it to.
      if(is_reference_type(type))
      {
        refuse_command(name, subject, "is a reference, which a command cannot bind", where);
      }

      const auto annotations = annotations_of(param);

      // A sub-command is descended into and called. A function parameter is not
      // something to descend into, so both the annotated form and the implicit
      // one are refused instead of becoming an unreachable sub-command.
      bool is_sub_command = annotations.empty()
                        and is_class_type(type)
                        and meta::has_annotation(type, ^^command);
      for(auto a : annotations)
      {
        is_sub_command |= decay(type_of(constant_of(a))) == ^^command;
      }
      if(is_sub_command)
      {
        refuse_command(name, subject, "names a sub-command, and sub-commands are struct only", where);
      }

      members.push_back(
          std::meta::data_member_spec(type, {.name = param_name, .annotations = annotations}));
    }
    return members;
  }

  /** @brief the command annotation carried by @p Fn, or a default one */
  consteval auto command_annotation_of(std::meta::info Fn) -> command
  {
    try
    {
      return meta::annotation_value_of_with<command>(Fn);
    }
    catch(std::meta::exception const&)
    {
      return {};
    }
  }

  /** @brief holder for the aggregate a function command is parsed into
   *
   * define_aggregate has to be evaluated from a consteval block with no scope
   * between the block and the type it completes, which rules out completing a
   * namespace-scope type from inside a function. A member class of the same
   * class template as the block satisfies it, and gives the aggregate a name
   * that varies with Fn. That last part is load bearing: reflections of the
   * members of two same-named local classes are interchanged by
   * define_static_array under GCC 16, so a local aggregate would let two
   * commands declaring the same parameters read each other's members.
   */
  template <std::meta::info Fn> struct command_of
  {
    static constexpr auto function = Fn;

    struct args;
    consteval { std::meta::define_aggregate(^^args, command_member_specs<Fn>()); }
  };

  /** @brief the aggregate @p Fn is parsed into, one member per parameter */
  template <std::meta::info Fn> using command_args = typename command_of<Fn>::args;

  /** @brief the command annotation describing @p I
   *
   * A synthesized aggregate never carries one of its own: an annotation written
   * on a declaration completed by define_aggregate is dropped, under GCC 16,
   * whenever the declaration sits in a template. The holder that declared it
   * names the function, and the function is what the user annotated.
   */
  consteval auto command_annotation_for(std::meta::info I) -> command
  {
    const auto holder = parent_of(dealias(I));
    if(is_type(holder) and has_template_arguments(holder) and template_of(holder) == ^^command_of)
    {
      return command_annotation_of(extract<std::meta::info>(template_arguments_of(holder)[0]));
    }
    return command_annotation_of(I);
  }

  template <std::meta::info I, bool include_install_completion = true> constexpr auto raw_parse()
  {
    std::vector<std::meta::info> arguments;
    std::vector<std::meta::info> options;
    std::vector<std::meta::info> sub_commands;

    options.push_back(^^help_option);
    if constexpr(include_install_completion)
    {
      options.push_back(^^install_completion_option);
      options.push_back(^^show_completion_option);
    }

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

  template <std::meta::info I, bool include_install_completion = true> constexpr auto parse()
  {
    static constexpr auto [arguments, options, sub_commands] =
        raw_parse<I, include_install_completion>();
    static constexpr auto args   = argument_info::from_info_range(arguments);
    static constexpr auto opts   = option_info::from_info_range(options);
    static constexpr auto s_cmds = command_info::from_info_range(sub_commands);
    return std::tuple{args, opts, s_cmds};
  }

  consteval std::string_view display_name_of(std::meta::info mem)
  {
    return constant_string{caseconv::to_kebab_case(identifier_of(mem))};
  }

  template <std::meta::info I, bool include_install_completion = true>
  void usage_of(std::string_view program)
  {
    static constexpr auto description          = command_annotation_for(I);
    static constexpr auto [args, opts, s_cmds] = parse<I, include_install_completion>();

    static constexpr std::size_t min_id_size = 16;

    if(auto pos = program.find_last_of("/"); pos != std::string_view::npos)
    {
      program.remove_prefix(pos + 1);
    }

    std::println("USAGE: {} [OPTIONS...] ARGUMENTS...", program);

    if constexpr(constexpr auto help = description.help; not help->empty())
    {
      std::println();
      std::println("{}", *help);
      std::println();
    }

    {
      std::println("OPTIONS:");

      std::size_t max_id_size = min_id_size;
      template for(constexpr auto opt : opts)
      {
        constexpr auto sz = [&]() consteval {
          if(opt.switches.s->empty())
          {
            return opt.switches.l->size();
          }
          if(opt.switches.l->empty())
          {
            return opt.switches.s->size();
          }
          return opt.switches.s->size() + opt.switches.l->size() + 1;
        }();
        max_id_size = std::max(max_id_size, sz);
      }
      template for(constexpr auto opt : opts)
      {
        constexpr auto [s, l] = opt.switches;
        auto switches_str     = [&]() {
          if(s->empty())
          {
            return std::string{*l};
          }
          if(l->empty())
          {
            return std::string{*s};
          }
          return std::format("{}/{}", *s, *l);
        }();
        std::println("  {:{}} {}", switches_str, max_id_size, *opt.help());
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
        std::println("  {:{}} {}", a.display_name(), max_id_size, *a.help());
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
        std::println("  {:{}} {}", c.display_name(), max_id_size, *c.help());
      }
    }

    std::println();
  }

  enum class parsing_state
  {
    completed,
    completion_check,
    invalid_argument_value,
    invalid_option_value,
    missing_option,
    missing_option_value,
    option_value_check,
    missing_argument,
    missing_command,
    unknown_option,
    unexpected_argument,
  };

  template <constant items> struct item_tracker
  {
    static constexpr auto N = items->size();
    std::array<bool, N>   _used{};

    using value_type = typename decltype(items)::type::value_type;

    constexpr void mark_used(std::size_t idx)
    {
      _used[idx] = true;
    }

    template <value_type value> constexpr void mark_used()
    {
      template for(constexpr auto ii : std::views::iota(0uz, N))
      {
        if constexpr(value == items->at(ii))
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
        if constexpr(value == items->at(ii))
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
          fn.template operator()<items->at(ii)>();
        }
      }
    }

    auto first_unused([[maybe_unused]] auto fn) const
    {
      template for(constexpr auto ii : std::views::iota(0uz, N))
      {
        if(not _used[ii])
        {
          fn.template operator()<items->at(ii)>();
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
          fn.template operator()<items->at(ii)>();
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
        fn.template operator()<items->at(ii)>();
      }
    }
  };

  template <constant items> constexpr auto make_tracker()
  {
    return item_tracker<items>{};
  }

  template <typename Cmd, bool include_install_completion = true> struct parse_trackers
  {
    static constexpr auto cmd_type = remove_cvref(^^Cmd);

    static constexpr auto _raw = raw_parse<cmd_type, include_install_completion>();

    static constexpr constant<std::vector<argument_info>> args =
        argument_info::from_info_range(std::get<0>(_raw)) | std::ranges::to<std::vector>();
    static constexpr constant<std::vector<option_info>> opts =
        option_info::from_info_range(std::get<1>(_raw)) | std::ranges::to<std::vector>();
    static constexpr constant<std::vector<command_info>> cmds =
        command_info::from_info_range(std::get<2>(_raw)) | std::ranges::to<std::vector>();

    Cmd& root;

    item_tracker<args> args_track{};
    item_tracker<opts> opts_track{};
    item_tracker<cmds> cmds_track{};

    struct _current
    {
      std::string_view view;
      std::string_view value_view;
      std::errc        parse_error = {};

      bool is_option() const
      {
        return not view.empty() and view[0] == '-';
      }

    } current{};

    void init_current(std::string_view v)
    {
      current.view        = v;
      current.value_view  = {};
      current.parse_error = {};
    }

    std::string_view command{};
    std::string_view program{};
    parsing_state    state = parsing_state::completed;
    std::size_t      index = 1;

    void usage() const
    {
      usage_of<cmd_type, include_install_completion>(command);
    }
  };

  /** @brief how a parsed command is run
   *
   * A hand written command carries its state in its members and is called with
   * nothing. A synthesized one carries a function's arguments instead and has
   * no call operator at all, so what it means to call it is supplied alongside
   * it rather than found on it.
   *
   * The constraint is what keeps a command holding nothing but sub-commands
   * reporting missing_command at run time instead of failing to compile.
   */
  inline constexpr auto default_invoker = []<typename C>(C& cli) -> decltype(auto)
    requires requires { cli(); }
  {
    return cli();
  };

  /** @brief call @p Fn with the arguments parsed into its aggregate */
  template <std::meta::info Fn>
  inline constexpr auto function_invoker = [](auto& cli) -> decltype(auto) {
    // to_tuple returns by value, so the pack binds by forwarding reference.
    return std::apply(
        [](auto&&... args) -> decltype(auto) { return [:Fn:](args...); }, reflex::to_tuple(cli));
  };

  template <
      bool show_help = true, bool include_install_completion = true, typename Cli,
      typename Invoker = decltype(default_invoker)>
  int process_cmdline(
      Cli&&            cli,
      std::string_view command,
      std::string_view executable,
      auto             it,
      auto             end,
      auto             state_handler,
      std::size_t      index   = 1,
      Invoker          invoker = default_invoker)
  {
    static constexpr auto                           cli_type = remove_cvref(^^Cli);
    parse_trackers<Cli, include_install_completion> trackers{cli};
    trackers.command = command;
    trackers.program = executable.empty() ? command : executable;
    trackers.index   = index;

    bool        treat_as_argument = false;
    std::size_t current_pos_arg   = 0;
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
        template for(constexpr auto o : *trackers.opts)
        {
          auto [short_switch, long_switch] = o.switches;

          if((trackers.current.view == *short_switch)
             or (trackers.current.view == *long_switch)
             or
             // counters accepts repeated short switch (ie.: -vvv => counter = 3)
             (o.is_counter()
              and not short_switch->empty()
              and trackers.current.view.starts_with(short_switch)
              and (std::ranges::all_of(trackers.current.view | std::views::drop(2), [&](auto c) {
                    return c == short_switch->at(1);
                  }))))
          {
            if constexpr(o == ^^help_option)
            {
              if(show_help)
              {
                usage_of<cli_type, include_install_completion>(trackers.program);
              }
              return 0;
            }
            else if constexpr(o == ^^install_completion_option)
            {
              std::string_view shell{};
              if(it != end and ++it != end)
              {
                ++trackers.index;
                auto next = std::string_view{*it};
                if(not next.empty() and next[0] != '-')
                {
                  shell = next;
                  ++it;
                  ++trackers.index;
                }
              }
              return install_completion(trackers.program, shell);
            }
            else if constexpr(o == ^^show_completion_option)
            {
              std::string_view shell{};
              if(it != end and ++it != end)
              {
                ++trackers.index;
                auto next = std::string_view{*it};
                if(not next.empty() and next[0] != '-')
                {
                  shell = next;
                  ++it;
                  ++trackers.index;
                }
              }
              return emit_completion(trackers.program, shell);
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
                    target.value() +=
                        std::ranges::count(trackers.current.view, short_switch->at(1));
                  }
                  else
                  {
                    target += T(std::ranges::count(trackers.current.view, short_switch->at(1)));
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
                    auto parsed =
                        reflex::parse_strict<typename T::value_type>(trackers.current.value_view);
                    if(not parsed)
                    {
                      trackers.current.parse_error = parsed.error();
                      trackers.state               = parsing_state::invalid_option_value;
                      state_handler(trackers);
                      return 1;
                    }
                    target.push_back(std::move(parsed).value());
                  }
                  else
                  {
                    auto parsed = reflex::parse_strict<T>(trackers.current.value_view);
                    if(not parsed)
                    {
                      trackers.current.parse_error = parsed.error();
                      trackers.state               = parsing_state::invalid_option_value;
                      state_handler(trackers);
                      return 1;
                    }
                    target = std::move(parsed).value();
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
          // In completion mode, a bare '-' is an option probe and should list
          // available switches instead of being consumed as a positional string.
          if constexpr(not show_help)
          {
            if(trackers.current.view == "-")
            {
              trackers.state = parsing_state::unknown_option;
              state_handler(trackers);
              return 1;
            }
          }

          // If a positional argument is still expected, accept values prefixed with '-'
          // (for example negative numbers) as arguments instead of unknown options.
          if(current_pos_arg < trackers.args->size())
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
        template for(constexpr auto cmd : *trackers.cmds)
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
            return process_cmdline<show_help, false>(
                cli.[:cmd.member:], std::format("{} {}", trackers.program, trackers.current.view),
                                  trackers.program, it, end, state_handler, trackers.index);
          }
        }

        // assume argument
        bool found = false;
        template for(constexpr auto ii : std::views::iota(std::size_t(0), trackers.args->size()))
        {
          if(ii >= current_pos_arg)
          {
            ++current_pos_arg;
            constexpr auto arg  = argument_info{trackers.args->at(ii)};
            constexpr auto type = arg.type();
            using T             = [:type:];
            T& target           = cli.[:arg.member:];
            trackers.args_track.template mark_used<arg>();

            if constexpr(seq_c<T>)
            {
              const_assert(
                  ii == trackers.args->size() - 1, "repeated arguments must be last",
                  arg.source_location());
              do
              {
                // consume all remaining arguments
                auto view             = std::string_view(*it);
                trackers.current.view = view;
                auto parsed           = reflex::parse_strict<typename T::value_type>(view);
                if(not parsed)
                {
                  trackers.current.parse_error = parsed.error();
                  trackers.state               = parsing_state::invalid_argument_value;
                  state_handler(trackers);
                  return 1;
                }
                target.push_back(std::move(parsed).value());
                ++trackers.index;
              } while(++it != end);
            }
            else
            {
              auto view             = std::string_view(*it);
              trackers.current.view = view;
              auto parsed           = reflex::parse_strict<T>(view);
              if(not parsed)
              {
                trackers.current.parse_error = parsed.error();
                trackers.state               = parsing_state::invalid_argument_value;
                state_handler(trackers);
                return 1;
              }
              target = std::move(parsed).value();
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

    constexpr auto arg_count = trackers.args->size();
    if(current_pos_arg < arg_count)
    {
      template for(constexpr auto ii : std::views::iota(std::size_t(0), trackers.args->size()))
      {
        if(ii >= current_pos_arg)
        {
          constexpr auto arg  = argument_info{trackers.args->at(ii)};
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

    if constexpr(requires { invoker(cli); })
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