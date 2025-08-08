#pragma once

#include <reflex/enum.hpp>
#include <reflex/meta.hpp>
#include <reflex/testing/context.hpp>
#include <reflex/testing/parametrize.hpp>
#include <reflex/to_tuple.hpp>
#include <reflex/visit.hpp>

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
           std::views::filter(
               [](auto R)
               {
                 return (is_function(R) or is_function_template(R) or (is_type(R) and is_class_type(R))) and
                        identifier_of(R).contains("test");
               }) |
           std::ranges::to<std::vector>();
  }
  else
  {
    return meta::member_functions_of(S) |
           std::views::filter([](auto M) { return not is_constructor(M) and identifier_of(M).contains("test"); }) |
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

struct any_arg
{
  template <typename T> operator T&();
};

consteval bool is_tuple_type(meta::info R)
{
  const auto RR = decay(R);
  return has_template_arguments(RR) and (template_of(RR) == ^^std::tuple);
}

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
  template <meta::info S, meta::info C, meta::info dummy = meta::null, size_t arg_count = 0> static void do_call_test()
  {
    const auto caller = []
    {
      if constexpr(is_namespace(S))
      {
        return []<typename... Args>(Args&&... args)
        {
          if constexpr(is_function_template(C))
          {
            visit(
                []<typename... Ts>(Ts&&... args)
                {
                  constexpr auto instanciation = substitute(C, std::array{^^Ts...});
                  const auto     ctx           = detail::fn_eval_context<instanciation>{args...};
                  return [:instanciation:](std::forward<Ts>(args)...);
                },
                std::forward<Args>(args)...);
          }
          else
          {
            const auto ctx = detail::fn_eval_context<C>{args...};
            return [:C:](std::forward<Args>(args)...);
          }
        };
      }
      else
      {
        using CaseT        = [:S:];
        using context_type = detail::class_eval_context<S>;
        return []<typename... Args>(Args&&... args)
        {
          auto       obj           = CaseT{};
          const auto ctx           = context_type{obj};
          detail::current_instance = &obj;

          if constexpr(is_function_template(C))
          {
            visit(
                [&obj]<typename... Ts>(Ts&&... args)
                {
                  constexpr auto instanciation = substitute(C, std::array{^^Ts...});
                  const auto     ctx           = detail::fn_eval_context<instanciation>{args...};
                  return obj.[:instanciation:](std::forward<Ts>(args)...);
                },
                std::forward<Args>(args)...);
          }
          else
          {
            const auto ctx = detail::fn_eval_context<C>{args...};
            obj.[:C:](args...);
          }
          detail::current_instance = nullptr;
        };
      }
    }();
    constexpr auto params_annotations = define_static_array(
        meta::annotations_of_with(dummy == meta::null ? C : dummy, ^^detail::parametrize_annotation));
    if constexpr(params_annotations.size())
    {
      static_assert(params_annotations.size() == 1, "only 1 parametrize annotation supported");

      constexpr auto annotation = constant_of(params_annotations[0]);

      constexpr auto fixture = [:annotation:].fixture;
      if constexpr(is_tuple_type(type_of(fixture)))
      {
        template for(const auto row : [:fixture:])
        {
          // const auto [... args] = row;
          // caller(std::move(args)...);
          std::apply(caller, std::move(row));
        }
      }
      else
      {
        for(const auto [... args] : [:fixture:])
        {
          caller(std::move(args)...);
        }
      }
    }
    else
    {
      caller();
    }
  }

  template <meta::info S> static void parse_suite(test_suite& root)
  {
    template for(constexpr auto C : define_static_array(get_cases(S)))
    {
      if constexpr(is_function(C))
      {
        static constexpr auto suite = S;
        static constexpr auto caze  = C;
        root.tests.push_back(test_case{
            .name  = std::string{display_string_of(C)},
            .loc   = source_location_of(C),
            .flags = get_flags(C),
            .fn    = [] { do_call_test<suite, caze>(); },
        });
      }
      else if constexpr(is_function_template(C))
      {
        static constexpr auto Tmpl      = C;
        static constexpr auto arg_count = []
        {
          static constexpr auto max_args = 16uz;
          template for(constexpr auto ii : std::views::iota(1uz, max_args))
          {
            constexpr auto n = max_args - ii - 1;
            if constexpr(can_substitute(Tmpl, std::views::repeat(^^any_arg) | std::views::take(n)))
            {
              return n;
            }
          }
          std::unreachable();
        }();

        static constexpr auto dummy = substitute(Tmpl, std::views::repeat(^^any_arg) | std::views::take(arg_count));

        static constexpr auto tsuite = S;
        static constexpr auto tcaze  = C;
        root.tests.push_back(test_case{
            .name  = std::string{display_string_of(C)},
            .loc   = source_location_of(C),
            .flags = get_flags(dummy),
            .fn    = [] { do_call_test<tsuite, tcaze, dummy, arg_count>(); },
        });
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
    if constexpr(is_namespace(S))
    {
      template for(constexpr auto SubSuite : define_static_array(get_suites(S)))
      {
        test_suite subsuite{
            .name  = std::string{display_string_of(SubSuite)},
            .loc   = meta::source_location_of(SubSuite),
            .flags = get_flags(SubSuite),
        };
        parse_suite<SubSuite>(subsuite);
        root.tests.push_back(subsuite);
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
