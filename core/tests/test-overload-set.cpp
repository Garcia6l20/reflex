#include <doctest/doctest.h>

import reflex.core;

using namespace reflex;

template <constant os> void show_overload_set()
{
  if constexpr(os->empty())
  {
    std::println("empty");
  }
  else
  {
    template for(constexpr auto s : *os)
    {
      static constexpr constant_string args_str = [&] consteval {
        std::string result;
        if constexpr(not s.parameter_types->empty())
        {
          template for(constexpr auto t : *s.parameter_types)
          {
            if(!result.empty())
            {
              result += ", ";
            }
            result += display_string_of(t);
          }
        }
        return result;
      }();
      std::println("({}) -> {}", args_str, display_string_of(s.return_type));
    }
  }
}

void f1()
{}

void f2(int)
{}

void f3([[maybe_unused]] int v = 42)
{}

void f4(
    [[maybe_unused]] std::string const& s,
    [[maybe_unused]] int                v = 42,
    [[maybe_unused]] double             d = 3.14)
{}

using namespace std::literals;

double f5(int a, double b = 1.0, std::string_view c = "default"sv)
{
  return a + b + c.size();
}

TEST_CASE("reflex::core::overload_set: functions")
{
  SUBCASE("no args")
  {
    std::println("====== f1 ======");
    constexpr auto os = overload_set_of(^^f1);
    show_overload_set<os>();
    static_assert(os.is_invocable({}));
  }
  SUBCASE("one int arg")
  {
    std::println("====== f2 ======");
    constexpr auto os = overload_set_of(^^f2);
    show_overload_set<os>();
    static_assert(os.size() == 1);
    static_assert(os.is_invocable({^^int}));
  }
  SUBCASE("one defaulted int arg")
  {
    std::println("====== f3 ======");
    constexpr auto os = overload_set_of(^^f3);
    show_overload_set<os>();
    static_assert(os.size() == 2);
    static_assert(os.is_invocable({}));
    static_assert(os.is_invocable({^^int}));
  }

  SUBCASE("two defaulted args")
  {
    std::println("====== f4 ======");
    constexpr auto os = overload_set_of(^^f4);
    static_assert(os.size() == 3);
    static_assert(os.is_invocable({^^std::string const& }));
    static_assert(os.is_invocable({^^std::string const&, ^^int}));
    static_assert(os.is_invocable({^^std::string const&, ^^int, ^^double}));
    show_overload_set<os>();
  }

  SUBCASE("f5")
  {
    constexpr auto os = overload_set_of(^^f5);
    static_assert(os.size() == 3);
    static_assert(os.is_invocable({^^int}));
    static_assert(os.is_invocable({^^int, ^^double}));
    static_assert(os.is_invocable({^^int, ^^double, ^^std::string_view}));
    static_assert(os.invoke_result({^^int}) == ^^double);
    static_assert(os.is_invocable_with(42, 3.14, "default"sv));
    int a1 = 42;
    static_assert(os.is_invocable_with(a1));
    std::println("====== f5 ======");
    show_overload_set<os>();
  }
}

struct S1
{
  S1(int)
  {}
  S1(const S1&) = delete;
  S1(S1&&)      = delete;
};

struct S2
{
  S2(int = 42)
  {}
  S2(const S2&) = delete;
  S2(S2&&)      = delete;
};

TEST_CASE("reflex::core::overload_set: ctors")
{
  SUBCASE("S1")
  {
    constexpr auto os = overload_set_of_type(^^S1);
    std::println("====== S1 ======");
    show_overload_set<os>();
    static_assert(os.size() == 1);
    static_assert(os.is_invocable({^^int}));
    static_assert(os.invoke_result({^^int}) == ^^S1);
  }
  SUBCASE("S2")
  {
    constexpr auto os = overload_set_of_type(^^S2);
    std::println("====== S2 ======");
    show_overload_set<os>();
    static_assert(os.size() == 2);
    static_assert(os.is_invocable({}));
    static_assert(os.is_invocable({^^int}));
  }
}