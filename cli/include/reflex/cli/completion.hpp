#pragma once

/** @file cli-completion.cppm
 *
 * @brief Dynamic completion
 *
 * Protocol (environment variables read by the binary):
 *   _REFLEX_COMPLETE=xxx_complete  - activates completion mode
 *   _REFLEX_COMP_LINE="git pu"     - full command line typed so far
 *   _REFLEX_COMP_POINT=6           - number of completed words
 *
 */

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <reflex/cli/base.hpp>
#include <reflex/heapless/string.hpp>

#include <cctype>
#include <inplace_vector>
#include <optional>
#endif

REFLEX_EXPORT namespace reflex::cli
{
  struct complete
  {
    std::meta::info completer{};
  };

} // namespace reflex::cli

REFLEX_EXPORT namespace reflex::cli::detail
{
  consteval std::meta::info completer_of(std::meta::info mem)
  {
    for(auto ann : annotations_of(mem))
    {
      const auto val      = constant_of(ann);
      const auto val_type = decay(type_of(val));

      // Case 1: direct [[= complete{...}]] annotation on the member
      if(val_type == ^^complete)
      {
        auto comp = extract<complete const&>(val);
        // explicit completer function stored: [[= complete{^^my_fn}]]
        if(comp.completer != meta::null)
          return comp.completer;
        // bare complete{} with no payload — shouldn't happen at member level
        return meta::null;
      }

      // Case 2: [[= path]] / [[= path.dirs]] / [[= path.files.matching("*.json")]]
      // The annotation value's TYPE is itself annotated with [[= complete{}]]
      if(meta::has_annotation(val_type, ^^complete))
      {
        // Return the annotation VALUE itself (the completer object, e.g. path)
        // so the caller can splice-call it: [:completer_of(mem):](current)
        return val;
      }
    }
    return meta::null; // no completer found
  }

  template <auto arg, configuration config> struct arg_completer
  {
    static constexpr auto comp     = completer_of(arg.member);
    static constexpr bool has_comp = comp != meta::null;
    static auto complete([[maybe_unused]] auto&& cli, std::string_view current, bool& completed)
    {
      if constexpr(has_comp)
      {
        const auto unwrap_result = [&](auto&& result) {
          if constexpr(requires {
                         std::get<0>(result) == false;
                         std::get<1>(result);
                       })
          {
            if(std::get<0>(result) == false)
            {
              completed = false;
            }
            return std::get<1>(result);
          }
          else
          {
            return result;
          }
        };
        if constexpr(requires { [:comp:](current); })
        {
          return unwrap_result([:comp:](current));
        }
        else if constexpr(requires { [:comp:](cli, current); })
        {
          return unwrap_result([:comp:](cli, current));
        }
        else if constexpr(requires { cli.[:comp:](current); })
        {
          return unwrap_result(cli.[:comp:](current));
        }
        else
        {
          static_assert(false, "Completer must be callable with (std::string_view)");
        }
      }
      else
      {
        return std::array<completion<config>, 0>{};
      }
    }
  };

  template <configuration config = {}, typename Cli>
  auto complete_for(Cli && cli, word_vector<config> words, std::size_t comp_point)
  {
    bool                      terminated = true;
    completion_vector<config> completions{};
    using completion_value_type       = typename completion<config>::value_type;
    using completion_description_type = typename completion<config>::description_type;
    cli::detail::process_cmdline<false>(
        cli, identifier_of(decay(^^Cli)), words.begin(), words.end(), [&](auto const& trackers) {
          const auto  state      = trackers.state;
          const auto  view       = trackers.current.view;
          const auto  value_view = trackers.current.value_view;
          const auto  index      = trackers.index;
          const auto& cmd        = trackers.root;

          if(state == cli::detail::parsing_state::missing_command)
          {
            trackers.cmds_track.unused([&]<auto cmd> {
              constexpr auto dname = cmd.display_name();
              if(dname.starts_with(view))
              {
                completions.push_back(
                    completion<config>{
                        .value       = static_cast<completion_value_type>(dname),
                        .description = static_cast<completion_description_type>(cmd.help())});
              }
            });
          }
          else if(state == cli::detail::parsing_state::unknown_option)
          {
            // offer unused options
            trackers.opts_track.unused([&]<auto opt> {
              constexpr auto [short_sw, long_sw] = opt.switches;
              if(short_sw.starts_with(view))
              {
                completions.push_back(
                    completion<config>{
                        .value       = static_cast<completion_value_type>(short_sw),
                        .description = static_cast<completion_description_type>(opt.help())});
              }
              if(long_sw.starts_with(view))
              {
                completions.push_back(
                    completion<config>{
                        .value       = static_cast<completion_value_type>(long_sw),
                        .description = static_cast<completion_description_type>(opt.help())});
              }
            });
          }
          else if(state == cli::detail::parsing_state::missing_argument)
          {
            // If the parser index already moved past the completion point, we are
            // still completing the current (partially typed) argument.
            if(index > comp_point)
            {
              trackers.args_track.last_used([&]<auto arg> {
                completions.append_range(
                    arg_completer<arg, config>::complete(cmd, view, terminated));
              });
            }
            else
            {
              trackers.args_track.first_unused([&]<auto arg> {
                completions.append_range(
                    arg_completer<arg, config>::complete(cmd, view, terminated));
              });
            }
          }
          else if(state == cli::detail::parsing_state::missing_option_value)
          {
            trackers.opts_track.all([&]<auto opt> {
              constexpr auto [short_sw, long_sw] = opt.switches;
              if(view == *short_sw or view == *long_sw)
              {
                if(index >= comp_point)
                {
                  if(view == *short_sw)
                  {
                    completions.push_back(
                        completion<config>{
                            .value       = static_cast<completion_value_type>(short_sw),
                            .description = static_cast<completion_description_type>(opt.help())});
                  }
                  else if(view == *long_sw)
                  {
                    completions.push_back(
                        completion<config>{
                            .value       = static_cast<completion_value_type>(long_sw),
                            .description = static_cast<completion_description_type>(opt.help())});
                  }
                }
                else
                {
                  completions.append_range(
                      arg_completer<opt, config>::complete(cmd, value_view, terminated)
                      | std::views::filter([value_view](auto const& c) {
                          return (c.type != cli::completion_type::plain)
                              or c.value.starts_with(value_view);
                        }));
                }
              }
            });
          }
          else if(state == cli::detail::parsing_state::invalid_option_value)
          {
            trackers.opts_track.unused([&]<auto opt> {
              constexpr auto [short_sw, long_sw] = opt.switches;
              if(view == *short_sw or view == *long_sw)
              {
                completions.append_range(
                    arg_completer<opt, config>::complete(cmd, value_view, terminated)
                    | std::views::filter([value_view](auto const& c) {
                        return (c.type != cli::completion_type::plain)
                            or ((value_view != c.value) and c.value.starts_with(value_view));
                      }));
              }
            });
          }
          else if(state == cli::detail::parsing_state::completion_check)
          {
            if(index >= comp_point)
            {
              trackers.cmds_track.all([&]<auto cmd> {
                if(cmd.display_name().starts_with(view))
                {
                  completions.push_back(
                      completion<config>{
                          .value       = static_cast<completion_value_type>(cmd.display_name()),
                          .description = static_cast<completion_description_type>(cmd.help())});
                }
              });
              return 1;
            }
          }
          else if(state == cli::detail::parsing_state::invalid_argument_value)
          {
            trackers.args_track.last_used([&]<auto arg> {
              completions.append_range(arg_completer<arg, config>::complete(cmd, view, terminated));
            });
          }
          else if(state == cli::detail::parsing_state::completed)
          {
            bool completed = false;

            trackers.args_track.last_used([&]<auto arg> {
              using completer = arg_completer<arg, config>;
              if constexpr(completer::has_comp)
              {
                completions.append_range(completer::complete(cmd, view, terminated));
                completed = true;
              }
            });

            if(completed and not view.empty())
            {
              return 0;
            }

            trackers.opts_track.unused([&]<auto opt> {
              constexpr auto [short_sw, long_sw] = opt.switches;
              completions.push_back(
                  completion<config>{
                      .value       = static_cast<completion_value_type>(long_sw),
                      .description = static_cast<completion_description_type>(opt.help())});
              completions.push_back(
                  completion<config>{
                      .value       = static_cast<completion_value_type>(short_sw),
                      .description = static_cast<completion_description_type>(opt.help())});
            });
          }
          else if(state == cli::detail::parsing_state::option_value_check)
          {
            // may be a partial completed option
            trackers.opts_track.unused([&]<auto opt> {
              constexpr auto [short_sw, long_sw] = opt.switches;
              if(view == *short_sw or view == *long_sw)
              {
                completions.append_range(
                    arg_completer<opt, config>::complete(cmd, value_view, terminated)
                    | std::views::filter([value_view](auto const& c) {
                        return (c.type != cli::completion_type::plain)
                            or ((value_view != c.value) and c.value.starts_with(value_view));
                      }));
              }
            });
          }
          else if(state == cli::detail::parsing_state::unexpected_argument)
          {
            if(index >= comp_point)
            {
              // probably a command
              trackers.cmds_track.unused([&]<auto cmd> {
                constexpr auto dname = cmd.display_name();
                if(dname.starts_with(view))
                {
                  completions.push_back(
                      completion<config>{
                          .value       = static_cast<completion_value_type>(dname),
                          .description = static_cast<completion_description_type>(cmd.help())});
                }
              });
              if(not completions.empty())
              {
                return 0;
              }
            }
            return 0;
          }
          else
          {
            std::unreachable();
          }
          return 0;
        });
    return std::make_tuple(terminated, completions);
  }

  constexpr std::optional<std::string_view> next_word(std::string_view & line)
  {
    line = reflex::ltrim(line);
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

  template <configuration Config = {}, typename Cmd>
  int do_complete(Cmd && cmd, std::string_view comp_line, std::size_t comp_point)
  {
    next_word(comp_line); // drop command name

    word_vector<Config> words;
    detail::tokenize(comp_line, std::back_inserter(words));
    // preserve a trailing empty word so completion runs for the current argument slot instead of
    // reusing the previous token.
    if(not comp_line.empty() and reflex::is_space(comp_line.back()))
    {
      words.push_back(std::string_view{});
    }
    const auto [completed, completions] = complete_for<Config>(cmd, words, comp_point);
    std::print("{}\n", completed ? '1' : '0');
    for(auto const& comp : completions)
    {
      comp.print();
    }
    return 0;
  }

  // Entry-point called from run() when _REFLEX_COMPLETE is set to xxx_complete.
  template <configuration Config = {}, typename Cmd> int do_complete(Cmd && cmd)
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

    return do_complete<Config>(cmd, comp_line, comp_point);
  }

  extern "C++" void emit_zsh_source(std::string_view executable);
  extern "C++" void emit_bash_source(std::string_view executable);
} // namespace reflex::cli::detail

REFLEX_EXPORT namespace reflex::cli::completers
{
  template <configuration config = {}> struct[[= complete{}]] path
  {
    constant_string pattern{"*"};

    auto operator()(std::string_view /* current */) const
    {
      return std::make_tuple(
          true, std::array{
                    cli::completion<config>{
                                            .type        = cli::completion_type::file,
                                            .value       = static_cast<cli::completion<config>::value_type>(pattern),
                                            .description = static_cast<cli::completion<config>::description_type>(
                            "File system path")}
      });
    }
  };

  template <enum_c E, configuration config = {}> struct[[= complete{}]] enumeration
  {
    constexpr auto operator()(std::string_view current) const
    {
      static constexpr auto names =
          define_static_array(enumerators_of(^^E) | std::views::transform([](auto e) {
                                return constant_string(identifier_of(e));
                              }));
      return std::make_tuple(
          true, names | std::views::filter([current](auto name) {
                  return current.empty() or name.starts_with(current);
                }) | std::views::transform([](auto name) {
                  return cli::completion<config>{
                      .type        = cli::completion_type::plain,
                      .value       = static_cast<cli::completion<config>::value_type>(name),
                      .description = static_cast<cli::completion<config>::description_type>(
                          identifier_of(^^E))};
                }));
    }
  };

} // namespace reflex::cli::completers
