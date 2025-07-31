#pragma once

#include <reflex/enum.hpp>
#include <reflex/meta.hpp>

#include <functional>
#include <print>
#include <source_location>
#include <string>
#include <variant>
#include <vector>

namespace reflex::testing
{

consteval auto get_suites(std::meta::info NS)
{
  return members_of(NS, meta::access_context::unchecked()) |
         std::views::filter([](auto R) { return is_namespace(R) and identifier_of(R).contains("test"); });
}

consteval auto get_cases(std::meta::info S)
{
  if(is_namespace(S))
  {
    return members_of(S, meta::access_context::unchecked()) |
           std::views::filter([](auto R)
                              { return (is_function(R) or is_class_type(R)) and identifier_of(R).contains("test"); }) |
           std::ranges::to<std::vector>();
  }
  else
  {
    // static_assert(is_class_type(S));
    return meta::member_functions_of(S) | std::views::filter([](auto M) { return identifier_of(M).contains("test"); }) |
           std::ranges::to<std::vector>();
  }
}

enum class test_flags
{
  none      = 0,
  fail_test = (1 << 0),
};

struct test_case
{
  std::string           name;
  std::source_location  loc;
  test_flags            flags;
  std::function<void()> fn;
};

struct test_suite
{
  std::string          name;
  std::source_location loc;
  test_flags           flags;
  using test_element = std::variant<test_suite, test_case>;
  std::vector<test_element> tests;

private:
  static consteval test_flags get_flags(meta::info R)
  {
    using namespace reflex::enum_bitwise_operators;
    test_flags flags = test_flags::none;
    if(detail::is_fail_test(R))
    {
      flags |= test_flags::fail_test;
    }
    return flags;
  }

  template <meta::info S> static void parse_suite(test_suite& root)
  {
    template for(constexpr auto C : define_static_array(get_cases(S)))
    {
      if constexpr(is_function(C))
      {
        if constexpr(is_namespace(S))
        {
          root.tests.push_back(test_case{
              .name  = std::string{display_string_of(C)},
              .loc   = source_location_of(C),
              .flags = get_flags(C),
              .fn    = [FN = [:C:]] { FN(); },
          });
        }
        else
        {
          using CaseT              = [:S:];
          static constexpr auto fn = C;
          root.tests.push_back(test_case{
              .name  = std::string{display_string_of(C)},
              .loc   = source_location_of(C),
              .flags = get_flags(C),
              .fn =
                  []
              {
                auto obj = CaseT{};
                obj.[:fn:]();
              },
          });
        }
      }
      else
      {
        test_suite caze{
            .name  = std::string{display_string_of(C)},
            .loc   = meta::source_location_of(C),
            .flags = get_flags(C),
        };
        parse_suite<C>(caze);
        root.tests.push_back(caze);
      }
    }
  }
  template <meta::info Ns> static void parse(test_suite& root)
  {
    constexpr auto suites = define_static_array(get_suites(Ns));
    template for(constexpr auto S : suites)
    {
      test_suite suite{
          .name = std::string{display_string_of(S)},
          .loc  = source_location_of(S),
      };
      parse_suite<S>(suite);
      root.tests.push_back(suite);
    }
  }

public:
  template <meta::info Ns> static test_suite parse()
  {
    test_suite root = {};
    parse<Ns>(root);
    return root;
  }
};
} // namespace reflex::testing

template <typename CharT>
struct std::formatter<reflex::testing::test_suite, CharT> : std::formatter<std::basic_string<CharT>, CharT>
{
  template <typename Ctx> auto format(reflex::testing::test_suite const& c, Ctx& ctx) const -> decltype(ctx.out())
  {
    return std::formatter<std::basic_string<CharT>, CharT>::format(c.name, ctx);
  }
};
