#include <reflex/cli.hpp>
#include <reflex/match.hpp>
#include <reflex/testing.hpp>
#include <reflex/testing/test_suite.hpp>

// #include <stacktrace>
#include <regex>

std::string fnmatch_to_regex(std::string_view pattern)
{
  std::string regex = "^";
  regex.reserve(pattern.size());
  for(char c : pattern)
  {
    switch(c)
    {
      case '*':
        regex += ".*";
        break;
      case '?':
        regex += ".";
        break;
      case '.':
        regex += "\\.";
        break;
      case '[':
      case ']':
        regex += c;
        break;
      default:
        if(std::isalnum(c))
        {
          regex += c;
        }
        else
        {
          regex += '\\';
          regex += c;
        }
    }
  }
  regex += "$";
  return regex;
}

inline auto make_fnmatch_impl(std::string_view pattern)
{
  return [re = std::regex(fnmatch_to_regex(pattern))](std::string_view str) //
  { return std::regex_match(str.data(), re); };
}
using fnmatch_t = decltype(make_fnmatch_impl(std::declval<std::string_view>()));

inline fnmatch_t make_fnmatch(std::string_view pattern)
{
  return make_fnmatch_impl(pattern);
}

bool fnmatch(std::string_view pattern, std::string_view str)
{
  return make_fnmatch(pattern)(str);
}

namespace reflex::testing
{
namespace detail
{
reporter_t& get_reporter()
{
  static reporter_t reporter;
  return reporter;
}
} // namespace detail

struct[[= cli::specs{"reflex test runner."}]] //
    command_line : cli::command
{
  command_line(test_suite const& suite) : cli::command{}, root{suite}
  {
  }

  test_suite const& root;

  using parent_vec = std::vector<std::reference_wrapper<const test_suite>>;

  template <typename Fn> void visit(Fn const& fn, parent_vec const& parents) const
  {
    for(const auto& test : parents.back().get().tests)
    {
      std::visit(
          match{
              [&](test_suite const& suite)
              {
                auto child_parents = parents;
                child_parents.push_back(suite);
                visit(fn, child_parents);
              },
              [&](test_case const& caze) { fn(parents | std::views::drop(1), caze); },
          },
          test);
    }
  }
  template <typename Fn> void visit(Fn const& fn) const
  {
    visit(fn, {root});
  }

  [[= cli::specs{"-v/--verbose", "Show more details."}, = cli::_count]] //
      int verbose = false;

  [[= cli::specs{"Run tests."}, = cli::_default]]                      //
      [[= cli::specs{":match:", "-m/--match", "Run matching tests."}]] //
      int exec(std::optional<std::string_view> match) const noexcept
  {
    std::optional<fnmatch_t> matcher;
    if(match.has_value())
    {
      matcher.emplace(make_fnmatch(match.value()));
    }
    visit(
        [&](const auto& parents, const test_case& caze)
        {
          const auto suite_name =
              parents                                                                                       //
              | std::views::transform([](auto const& suite) { return std::string_view{suite.get().name}; }) //
              | std::views::join_with(':') | std::ranges::to<std::string>();
          const auto test_id = std::format("{}:{}", suite_name, caze.name);
          if(matcher and not matcher.value()(test_id))
          {
            return;
          }
          std::println("Running {}...", test_id);
          try
          {
            caze.fn();
          }
          catch(std::exception const& err)
          {
            detail::get_reporter().append(suite_name,
                                          caze.name,
                                          detail::validation_result{
                                              .result     = false,
                                              .expression = std::format("case thrown an exception: {}", err.what()),
                                              // .expanded_expression = std::format("{}", std::stacktrace::current()),
                                              .location = source_location_of(^^caze),
                                              // .location = err.where(),
                                          });
          }
        });
    return detail::get_reporter().finish();
  }

  [[= cli::specs{"List tests."}]] //
      int discover() const noexcept
  {
    visit(
        [&](const auto& parents, const test_case& caze)
        {
          const auto suite_name =
              parents                                                                                       //
              | std::views::transform([](auto const& suite) { return std::string_view{suite.get().name}; }) //
              | std::views::join_with(':') | std::ranges::to<std::string>();
          std::print("{}:{}", suite_name, caze.name);
          using namespace enum_bitwise_operators;
          std::vector<std::string> flags;
          if(bool(caze.flags & test_flags::fail_test) or
             (std::ranges::any_of(parents,
                                  [](auto const& suite) { return bool(suite.get().flags & test_flags::fail_test); })))
          {
            flags.push_back("failing");
          }
          if(not flags.empty())
          {
            std::print("#{}", flags | std::views::join_with(':') | std::ranges::to<std::string>());
          }
          std::println();
        });
    return 0;
  }
};

int main(int argc, const char** argv, test_suite const& suite)
{
  command_line cl{suite};
  return cl.run(argc, argv);
}
} // namespace reflex::testing
