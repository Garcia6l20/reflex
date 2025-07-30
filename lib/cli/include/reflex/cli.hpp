#pragma once

#include <reflex/fixed_string.hpp>
#include <reflex/from_string.hpp>
#include <reflex/meta.hpp>
#include <reflex/named_tuple.hpp>

#include <charconv>
#include <meta>
#include <print>
#include <ranges>
#include <span>
#include <string_view>
#include <system_error>

namespace reflex::cli
{

class command;

namespace detail
{
enum class spec_kind
{
  unknown,
  option,
  argument,
  command,
};

struct specs_impl
{
  // spec_kind kind;
  std::string_view short_switch;
  std::string_view long_switch;
  std::string_view help;
  std::string_view field;
};

template <auto... Specs> struct specs
{
  static constexpr auto specs_ = std::make_tuple(Specs...);

  template <fixed_string s> static constexpr auto __get_opt_switches()
  {
    static constexpr auto npos  = std::string_view::npos;
    auto                  v     = s.view();
    constexpr auto        slash = s.find('/');

    std::string_view short_switch;
    std::string_view long_switch;

    if constexpr(slash != npos)
    {
      size_t short_begin = 0;
      size_t long_begin  = 0;
      if(v[1] == '-')
      {
        long_switch  = v.substr(0, slash);
        short_switch = v.substr(slash + 1);
      }
      else
      {
        short_switch = v.substr(0, slash);
        long_switch  = v.substr(slash + 1);
      }
    }
    else
    {
      // No slash
      if(v[1] == '-')
      {
        long_switch = v;
      }
      else
      {
        short_switch = v;
      }
    }
    return std::make_tuple(short_switch, long_switch);
  }

  static constexpr auto i = []() consteval
  {
    specs_impl i = {};

    constexpr auto ctx = std::meta::access_context::current();
    template for(constexpr auto s : std::make_tuple(Specs...))
    {
      constexpr auto type = type_of(^^s);
      if constexpr(template_of(type) == ^^fixed_string)
      {
        if constexpr(s[0] == '-')
        {
          std::tie(i.short_switch, i.long_switch) = __get_opt_switches<s>();
        }
        else if constexpr(s[0] == ':')
        {
          i.field = extract<s, 1, -1>();
        }
        else
        {
          // assume help
          i.help = extract<s>();
        }
      }
    }
    return i;
  }();

  static constexpr bool is_opt       = not specs::i.short_switch.empty() or not specs::i.long_switch.empty();
  static constexpr bool is_field     = not specs::i.field.empty();
  static constexpr auto short_switch = specs::i.short_switch;
  static constexpr auto long_switch  = specs::i.long_switch;
  static constexpr auto help         = specs::i.help;
  static constexpr auto field        = specs::i.field;
};

template <std::meta::info I, std::meta::info M> constexpr auto specs_of()
{
  if constexpr(is_function(I))
  {
    constexpr auto               fn_annotations = define_static_array(meta::template_annotations_of<I, ^^specs>());
    std::vector<std::meta::info> member_annotations;
    const auto                   name = identifier_of(M);
    template for(constexpr auto a : fn_annotations)
    {
      constexpr auto s = [:a:];
      if(s.field == name)
      {
        member_annotations.push_back(a);
      }
    }
    return define_static_array(member_annotations)[0];
  }
  else
  {
    return define_static_array(meta::template_annotations_of<M, ^^specs>())[0];
  }
}

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
  if constexpr(is_function(I))
  {
    std::vector<std::meta::info> member_annotations;
    constexpr auto               name          = identifier_of(M);
    constexpr auto               annotations   = define_static_array(annotations_of(I));
    size_t                       member_offset = 0;
    template for(constexpr auto ii : std::views::iota(size_t(0), annotations.size()))
    {
      constexpr auto A  = annotations[ii];
      constexpr auto AT = type_of(A);
      if constexpr(has_template_arguments(AT) and template_of(AT) == ^^detail::specs)
      {
        constexpr auto s = [:constant_of(A):];
        if constexpr(s.field == name)
        {
          template for(constexpr auto jj : std::views::iota(size_t(ii + 1), annotations.size()))
          {
            constexpr auto ATT  = type_of(annotations[jj]);
            constexpr auto PATT = parent_of(ATT);
            if constexpr(PATT == ^^detail::_flags)
            {
              constexpr auto FLG = remove_const(ATT);
              member_annotations.push_back(FLG);
            }
            else
            {
              return define_static_array(member_annotations);
            }
          }
        }
      }
    }
    return define_static_array(member_annotations);
  }
  else
  {
    return define_static_array( //
        annotations_of(M)       //
        | std::views::filter(
              [&](auto A)
              {
                auto AT     = type_of(A);
                auto parent = parent_of(AT);
                return parent == ^^_flags;
              })                                    //
        | std::views::transform(std::meta::type_of) //
        | std::views::transform(std::meta::remove_const));
  }
}

template <meta::info I, meta::info M, meta::info FLG> consteval bool has_flag()
{
  constexpr auto F = remove_const(type_of(FLG));
  return std::ranges::contains(flags_of<I, M>(), F);
}

template <std::meta::info I> static constexpr auto parse()
{
  constexpr auto               ctx = std::meta::access_context::current();
  std::vector<std::meta::info> options;
  std::vector<std::meta::info> arguments;
  std::vector<std::meta::info> commands;

  if constexpr(is_function(I))
  {
    constexpr auto specs_annotations = define_static_array(meta::template_annotations_of<I, ^^detail::specs>());
    template for(constexpr auto p : define_static_array(parameters_of(I)))
    {
      constexpr auto name = identifier_of(p);
      template for(constexpr auto a : specs_annotations)
      {
        constexpr auto s = [:a:];
        if(s.field == name)
        {
          if(s.is_opt)
          {
            options.push_back(p);
          }
          else
          {
            arguments.push_back(p);
          }
        }
      }
    }
  }
  else
  {
    constexpr auto members = define_static_array(
        members_of(I, std::meta::access_context::current()) |
        std::views::filter([](auto M)
                           { return is_nonstatic_data_member(M) or (is_user_declared(M) and is_function(M)); }));
    template for(constexpr auto mem : members)
    {
      constexpr auto specs_annotations = define_static_array(meta::template_annotations_of<mem, ^^detail::specs>());
      if constexpr(!specs_annotations.empty())
      {
        constexpr auto mem_type = type_of(mem);
        if constexpr([:specs_annotations[0]:].is_opt)
        {
          options.push_back(mem);
        }
        else if constexpr(is_function(mem))
        {
          commands.push_back(mem);
        }
        else if constexpr(is_class_type(type_of(mem)) and is_base_of_type(^^command, type_of(mem)))
        {
          commands.push_back(mem);
        }
        else
        {
          arguments.push_back(mem);
        }
      }
    }
    // lookup base classes arguments, options, commands...
    template for(constexpr auto base : define_static_array(bases_of(I, ctx)))
    {
      constexpr auto base_type = type_of(base);
      if constexpr(base_type != (^^command) and is_base_of_type(^^command, base_type))
      {
        constexpr auto items = parse<base_type>();
        options.append_range(std::get<0>(items));
        arguments.append_range(std::get<1>(items));
        commands.append_range(std::get<2>(items));
      }
    }
  }
  return std::make_tuple(define_static_array(options), define_static_array(arguments), define_static_array(commands));
}

template <std::meta::info I, int N = -1>
int invoke(auto fn, std::string_view program, std::forward_iterator auto it, std::forward_iterator auto end)
{
  static constexpr auto items     = detail::parse<I>();
  static constexpr auto options   = std::get<0>(items);
  static constexpr auto arguments = std::get<1>(items);

  template for(constexpr auto opt : options)
  {
    if constexpr(has_default_argument(opt))
    {
      using T = [:type_of(opt):];
      static_assert(always_false<T>,
                    "Defaulted argument are not supported in function-based cli... (use std::optional<...>)");
    }
  }

  struct values_t;
  consteval
  {
    std::vector<meta::info> values;
    values.append_range(arguments);
    values.append_range(options);

    std::vector<meta::info> members;
    for(auto item : values)
    {
      members.push_back(data_member_spec(type_of(item), {.name = identifier_of(item)}));
    }
    define_aggregate(^^values_t, members);
  }

  auto       values = make_named_tuple(values_t{});
  const auto rc     = process_cmdline<I>( //
      program,
      it,
      end,
      //
      // get item
      //
      [&]<meta::info mem, typename T>(std::string_view name) -> std::reference_wrapper<T>
      {
        constexpr auto type = type_of(mem);
        return std::ref(values.template get<T>(name));
      },
      //
      // exec
      //
      [&]<meta::info mem>(auto it, auto end)
      { throw std::runtime_error("weird: function-based command should not have subcommands"); });

  if(rc.has_value())
  {
    return rc.value();
  }

  return std::apply(fn, to_tuple(values));
}

template <std::meta::info I> static void usage_of(std::string_view program)
{
  static constexpr auto                             items     = parse<I>();
  static constexpr std::span<std::meta::info const> options   = std::get<0>(items);
  static constexpr std::span<std::meta::info const> arguments = std::get<1>(items);
  static constexpr std::span<std::meta::info const> commands  = std::get<2>(items);

  static constexpr size_t min_id_size = 16;

  if(auto pos = program.find_last_of("/"); pos != std::string_view::npos)
  {
    program.remove_prefix(pos + 1);
  }

  std::println("USAGE: {} [OPTIONS...] ARGUMENTS...", program);

  {
    std::println("OPTIONS:");

    size_t max_id_size = min_id_size;
    template for(constexpr auto mem : options)
    {
      constexpr auto mem_spec = [:detail::specs_of<I, mem>():];
      max_id_size             = std::max(max_id_size, mem_spec.short_switch.size() + mem_spec.long_switch.size() + 1);
    }
    std::println("  {:{}} {}", "-h/--help", max_id_size, "Print this message and exit.");
    template for(constexpr auto mem : options)
    {
      constexpr auto mem_spec     = [:detail::specs_of<I, mem>():];
      auto           switches_str = std::format("{}/{}", mem_spec.short_switch, mem_spec.long_switch);
      std::println("  {:{}} {}", switches_str, max_id_size, mem_spec.help);
      constexpr auto flags = detail::flags_of<I, mem>();
      if constexpr(flags.size())
      {
        std::print("    -> flags: ");
        template for(constexpr auto flg : flags)
        {
          std::print("{}, ", identifier_of(flg));
        }
        std::println();
      }
    }
  }

  if constexpr(not arguments.empty())
  {
    std::println("ARGUMENTS:");
    size_t max_id_size = min_id_size;
    template for(constexpr auto mem : arguments)
    {
      max_id_size = std::max(max_id_size, identifier_of(mem).size());
    }
    template for(constexpr auto mem : arguments)
    {
      constexpr auto mem_spec = [:detail::specs_of<I, mem>():];
      std::println("  {:{}} {}", identifier_of(mem), max_id_size, mem_spec.help);
    }
  }

  if constexpr(not commands.empty())
  {
    std::println("COMMANDS:");
    size_t max_id_size = min_id_size;
    template for(constexpr auto mem : commands)
    {
      max_id_size = std::max(max_id_size, identifier_of(mem).size());
    }
    template for(constexpr auto mem : commands)
    {
      constexpr auto mem_spec = [:detail::specs_of<I, mem>():];
      std::println("  {:{}} {}", identifier_of(mem), max_id_size, mem_spec.help);
    }
  }
}

template <int N = -1> consteval auto call_args_to_tuple_type(std::meta::info type) -> std::meta::info
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
                      parameters_of(type) | std::views::transform(std::meta::type_of) |
                          std::views::transform(remove_cvref) | std::ranges::to<std::vector>());
  }
  else
  {
    return substitute(^^std::tuple,
                      parameters_of(type) | std::views::take(N) | std::views::transform(std::meta::type_of) |
                          std::views::transform(remove_cvref) | std::ranges::to<std::vector>());
  }
}

template <meta::info I>
std::optional<int> process_cmdline(std::string_view program, auto it, auto end, auto get_item, auto on_command)
{
  static constexpr auto items     = detail::parse<I>();
  static constexpr auto options   = std::get<0>(items);
  static constexpr auto arguments = std::get<1>(items);
  static constexpr auto commands  = std::get<2>(items);

  size_t current_pos_arg = 0;
  while(it != end)
  {
    auto view = std::string_view{*it};
    if(view[0] == '-')
    {
      // option lookup
      bool found = false;
      if(view == "-h" or view == "--help")
      {
        detail::usage_of<I>(program);
        return 0;
      }
      template for(constexpr auto mem : options)
      {
        constexpr auto mem_spec   = [:detail::specs_of<I, mem>():];
        constexpr bool is_counter = detail::has_flag<I, mem, ^^_count>();

        if(view == mem_spec.short_switch or view == mem_spec.long_switch or
           // counters accepts repeated short switch (ie.: -vvv => counter = 3)
           (is_counter and view.starts_with(mem_spec.short_switch) and
            (std::ranges::all_of(view | std::views::drop(2), [&](auto c) { return c == mem_spec.short_switch[1]; }))))
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
            if constexpr(is_counter)
            {
              if constexpr(has_template_arguments(type) and template_of(type) == ^^std::optional)
              {
                if(not target.has_value())
                {
                  target.emplace();
                }
                target.value() += std::ranges::count(view, mem_spec.short_switch[1]);
              }
              else
              {
                target += std::ranges::count(view, mem_spec.short_switch[1]);
              }
            }
            else
            {
              it              = std::next(it);
              auto value_view = std::string_view{*it};
              target          = from_string<T>(value_view).value();
            }
          }

          found = true;
          break;
        }
      }
      if(!found)
      {
        std::println(stderr, "unknown option: {}", view);
        std::println();
        detail::usage_of<I>(program);
        return 1;
      }
    }
    else
    {
      template for(constexpr auto mem : commands)
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
          constexpr auto mem                 = arguments[ii];
          constexpr auto type                = type_of(mem);
          using T                            = [:type:];
          T& target                          = get_item.template operator()<mem, T>(identifier_of(mem));
          auto                          view = std::string_view(*it);
          if constexpr(type == ^^bool)
          {
            target = view == "true";
          }
          else
          {
            target = from_string<T>(view).value();
          }
          found = true;
          break;
        }
      }
      if(!found)
      {
        std::println(stderr, "unexpected argument: {}", view);
        std::println();
        detail::usage_of<I>(program);
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
        if constexpr(has_template_arguments(type) and template_of(type) == ^^std::optional)
        {
          ++current_pos_arg;
        }
        else
        {
          std::print(stderr, "missing required argument: ");
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
          detail::usage_of<I>(program);
          return 1;
        }
      }
    }
  }

  if constexpr(not commands.empty())
  {
    template for(constexpr auto mem : commands)
    {
      if constexpr(detail::has_flag<I, mem, ^^_default>())
      {
        return on_command.template operator()<mem>(it, end);
      }
    }
  }

  return std::nullopt;
}
} // namespace detail

template <fixed_string... Specs> constexpr detail::specs<Specs...> specs;

constexpr auto _count   = detail::_count;
constexpr auto _default = detail::_default;

class command
{
private:
  void* parent_ = nullptr;

public:
  template <typename Self> decltype(auto) parent(this Self& self)
  {
    constexpr auto I = ^^Self;
    using ParentT    = [:parent_of(I):];
    return *static_cast<ParentT*>(self.parent_);
  };

  template <typename Self> void usage(this Self const& self, std::string_view program)
  {
    detail::usage_of<^^Self>(program);
  }

  template <typename Self> int run(this Self& self, std::string_view program, auto it, auto end)
  {
    const auto rc = detail::process_cmdline<^^Self>( //
        program,
        it,
        end,
        //
        // Get item
        //
        [&]<meta::info mem, typename T>(std::string_view name) -> std::reference_wrapper<T>
        { return std::ref(self.[:mem:]); },
        //
        // On command
        //
        [&]<meta::info mem>(auto it, auto end)
        {
          constexpr auto type = type_of(mem);
          using T             = [:type:];
          auto view           = std::string_view(it != end ? *it : "");

          if constexpr(is_function(mem))
          {
            if(it != end and std::ranges::find_if(std::next(it), end, [](auto it) { return it == "-h" or it == "--help"; }) != end)
            {
              detail::usage_of<mem>(std::format("{} {}", program, view));
              return 0;
            }
            else
            {
              return detail::invoke<mem>([&self]<typename... Args>(Args&&... args)
                                         { return self.[:mem:](std::forward<Args>(args)...); },
                                         std::format("{} {}", program, view),
                                         it != end ? std::next(it) : it,
                                         end);
            }
          }
          else
          {
            self.[:mem:].parent_ = &self;
            return self.[:mem:].run(std::format("{} {}", program, view), std::next(it), end);
          }
        });
    if(rc.has_value())
    {
      return rc.value();
    }

    if constexpr(requires { self.operator()(); })
    {
      return self.operator()();
    }
    else
    {
      std::println("Error: missing subcommand");
      return -1;
    }
  }

public:
  template <typename Self> int run(this Self& self, std::string_view program, std::span<const char*> args)
  {
    return self.run(program, std::begin(args), std::end(args));
  }

  template <typename Self> int run(this Self& self, int argc, const char** argv)
  {
    auto program = std::string_view{argv[0]};
    if(auto pos = program.find_last_of("/"); pos != std::string_view::npos)
    {
      program.remove_prefix(pos + 1);
    }
    return self.run(program, std::span{argv + 1, size_t(argc - 1)});
  }
};

template <meta::info Cli> int run(std::string_view program, std::span<const char*> args)
{
  return detail::invoke<Cli>([]<typename... Args>(Args&&... args) { return [:Cli:](std::forward<Args>(args)...); },
                             program,
                             std::begin(args),
                             std::end(args));
}

template <meta::info Cli> int run(int argc, const char** argv)
{
  auto program = std::string_view{argv[0]};
  if(auto pos = program.find_last_of("/"); pos != std::string_view::npos)
  {
    program.remove_prefix(pos + 1);
  }
  return run<Cli>(program, std::span{argv + 1, size_t(argc - 1)});
}

} // namespace reflex::cli
