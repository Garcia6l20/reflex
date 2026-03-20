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
export module reflex.cli:completion;

import std;

import reflex.core;
import :base;

export namespace reflex::cli
{
struct complete
{
  std::meta::info completer{};
};

enum class completion_type
{
  plain, // simple completion with no description
  dir,   // complete with directories
  file   // complete with files
};

struct completion
{
  completion_type  type = completion_type::plain;
  std::string_view value;
  std::string_view description;

  void print() const
  {
    std::println("{}\n{}\n{}", type, value, description);
  }
};

} // namespace reflex::cli

export namespace reflex::cli::detail
{
using word_vector       = std::inplace_vector<std::string_view, 32>;
using completion_vector = std::inplace_vector<completion, 16>;
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

template <meta::info I> auto complete_for(word_vector words, std::string_view current)
{
  completion_vector completions{};

  static constexpr auto [args, opts, s_cmds] = parse<I>();

  // If there are still words before the cursor that are not the current one,
  // check whether the first of them names a sub-command we should recurse into.
  if(not words.empty())
  {
    template for(constexpr auto c : s_cmds)
    {
      auto it = std::ranges::find(words, c.display_name());
      if(it != words.end())
      {
        constexpr auto sub_type = c.type();
        return complete_for<sub_type>(word_vector{it + 1, words.end()}, current);
      }
    }
  }
  template for(constexpr auto c : s_cmds)
  {
    if(current == c.display_name())
    {
      constexpr auto sub_type = c.type();
      return complete_for<sub_type>(words, {});
    }
  }

  // Emit matching sub-command names.
  template for(constexpr auto c : s_cmds)
  {
    constexpr auto name = c.display_name();
    if(current.empty() or name.starts_with(current))
    {
      completions.push_back({.value = name, .description = c.help()});
    }
  }
  if(not current.empty() and not completions.empty())
  {
    return completions;
  }

  if(current.starts_with('-'))
  {
    // Emit matching option switches.
    template for(constexpr auto opt : opts)
    {
      constexpr auto [short_sw, long_sw] = opt.switches;
      if(std::ranges::find_if(words, [&](auto w) { return w == long_sw or w == short_sw; })
         != words.end())
      {
        continue; // already present in command line, don't offer again
      }

      constexpr auto descr = opt.help();
      if(current == long_sw)
      {
        return completion_vector{
            {.value = long_sw, .description = descr}
        };
      }
      else if(current == short_sw)
      {
        return completion_vector{
            {.value = short_sw, .description = descr}
        };
      }
      if(long_sw.starts_with(current))
      {
        completions.push_back({.value = long_sw, .description = descr});
      }
      if(short_sw.starts_with(current))
      {
        completions.push_back({.value = short_sw, .description = descr});
      }
    }
  }

  std::string_view last_word = words.empty() ? "" : words.back();
  if(completions.empty())
  {
    if(not last_word.empty())
    {
      template for(constexpr auto opt : opts)
      {
        constexpr auto comp = completer_of(opt.member);
        if constexpr(comp != meta::null)
        {
          constexpr auto [short_sw, long_sw] = opt.switches;
          if(last_word == short_sw or last_word == long_sw)
          {
            constexpr auto fn = comp;
            completions.append_range([:fn:](current));
            return completions;
          }
        }
      }
    }

    // at this point we should try for argument completion
    template for(constexpr auto a : args)
    {
      constexpr auto comp = completer_of(a.member);
      if constexpr(comp != meta::null)
      {
        constexpr auto fn         = comp;
        auto           candidates = [:comp:](current);
        for(auto candidate : candidates)
        {
          if(candidate.value.starts_with(current))
          {
            completions.push_back(candidate);
          }
        }
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

template <meta::info I> int do_complete(std::string_view comp_line, std::size_t comp_point)
{
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

  return do_complete<I>(comp_line, comp_point);
}

void emit_zsh_source(std::string_view program);
void emit_bash_source(std::string_view program);
} // namespace reflex::cli::detail

export namespace reflex::cli::completers
{
struct[[= complete{}]] path
{
  constant_string pattern{"*"};

  auto operator()(std::string_view current) const
  {
    return std::array{
        cli::completion{
                        .type        = cli::completion_type::file,
                        .value       = pattern,
                        .description = "File system path"}
    };
  }
};

} // namespace reflex::cli::completers
