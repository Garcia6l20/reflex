#include <reflex/var.hpp>

#include <exception>
#include <print>

using namespace reflex;

#include <reflex/testing_main.hpp>

namespace var_tests
{
struct test_basic
{
  using var_t = var<int, bool, std::string>;
  var_t c;

  test_basic()
  {
    CHECK_THAT(not c.has_value());
    CHECK_THAT(c.can_hold<int, bool>());
    CHECK_THAT(c.index_of<none_t>()) == 0;
    CHECK_THAT(c.index_of<int>()) == 1;
    CHECK_THAT(c.index_of<bool>()) == 2;
    CHECK_THAT(c.index_of<double>()) == -1;
    c = 42;
    CHECK_THAT(c.has_value());
    CHECK_THAT(c.has_value<int>());
  }

  void test_changing_value()
  {
    c = false;
    CHECK_THAT(c.has_value());
    CHECK_THAT(c.has_value<bool>());
    bool value = c;
    CHECK_THAT(value) == false;
  }
  void test_string_value()
  {
    CHECK_THAT(not c.has_value<std::string>());
    c = "hello";
    CHECK_THAT(c.has_value());
    CHECK_THAT(c.has_value<std::string>());
    std::println("{}", c);
    std::string value = c;
    CHECK_THAT(value) == "hello";
  }
  void test_matching_1()
  {
    const auto r = c.match([](auto&& value) -> var_t {//
      return {value};
    });
    CHECK_THAT(r) == expr(c);
    c = 55;
    CHECK_THAT(r) != expr(c);
    CHECK_THAT(r) == 42;
    const int value = r;
    CHECK_THAT(value) == 42;
    bool bvalue;
    CHECK_THAT_LAZY(bvalue = c).throws<bad_var_access>();
  }
  void test_matching_2()
  {
    const auto r = c.match(
      [](int const& value) {//
        return true;
      },
      [](auto const& value) {//
        return false;
      });
    CHECK_THAT(r);
  }
  void test_matching_3()
  {
    CHECK_THAT(std::move(c).match(
      [](int && value) {//
        return true;
      },
      [](auto && value) {//
        return false;
      }));
  }

  //// WARNING: following lines crashes clang...
  // struct S
  // {
  // };
  // c.match(
  //     [](S const& value) {//
  //       std::unreachable();
  //     });
  // SECTION("matching 4")
  // {
  //   c.match(
  //     [](std::string const& value) {//
  //       std::unreachable();
  //     });
  // }
};

void test_string()
{
  using var_t = var<int, bool, std::string>;
  STATIC_CHECK_THAT(not meta::has_explicit_constructor(^^int,
                                                       {
                                                           ^^const char* }));
  STATIC_CHECK_THAT(not meta::has_explicit_constructor(^^bool,
                                                       {
                                                           ^^const char* }));
  STATIC_CHECK_THAT(meta::has_explicit_constructor(^^std::string,
                                                   {
                                                       ^^const char* }));

  STATIC_CHECK_THAT(not std::equality_comparable_with<int, const char*>);
  STATIC_CHECK_THAT(not std::equality_comparable_with<bool, const char*>);
  STATIC_CHECK_THAT(std::equality_comparable_with<std::string, const char*>);
  STATIC_CHECK_THAT(var_t::equality_comparable_with<const char*>());
  // construct
  {
    var_t c{"hello constructed"};
    std::println("{}", c);
    CHECK_THAT(c) == "hello constructed";
  }
  // assign
  {
    var_t c = "hello assigned";
    std::println("{}", c);
    CHECK_THAT(c) == "hello assigned";
  }
}

template <typename T> struct lifetime_dumper
{
  struct
  {
    int move_count = 0;
    int copy_count = 0;
  } lifetime_info{};

  template <typename... Args> void __lifetime_info(std::format_string<Args...> fmt, Args&&... args) const noexcept
  {
    std::print("{}@{:p}: ", display_string_of(^^T), static_cast<const void*>(this));
    std::println(fmt, std::forward<Args>(args)...);
  }

  lifetime_dumper() noexcept
  {
    __lifetime_info("default-constructed");
  }

  ~lifetime_dumper() noexcept
  {
    __lifetime_info("destructed");
  }

  lifetime_dumper(lifetime_dumper&& o) noexcept : lifetime_info{std::move(o.lifetime_info)}
  {
    ++lifetime_info.move_count;
    __lifetime_info("move-constructed from {:p} ({} moves)", static_cast<const void*>(&o), lifetime_info.move_count);
  }

  lifetime_dumper(lifetime_dumper const& o) noexcept : lifetime_info{o.lifetime_info}
  {
    ++lifetime_info.copy_count;
    __lifetime_info("copy-constructed from {:p} ({} copies)", static_cast<const void*>(&o), lifetime_info.copy_count);
  }

  lifetime_dumper& operator=(lifetime_dumper&& o) noexcept
  {
    lifetime_info = std::move(o.lifetime_info);
    ++lifetime_info.move_count;
    __lifetime_info("move-assigned from {:p} ({} moves)", static_cast<const void*>(&o), lifetime_info.move_count);
    return *this;
  }

  lifetime_dumper& operator=(lifetime_dumper const& o) noexcept
  {
    lifetime_info = o.lifetime_info;
    ++lifetime_info.copy_count;
    __lifetime_info("copy-assigned from {:p} ({} copies)", static_cast<const void*>(&o), lifetime_info.copy_count);
    return *this;
  }
};

struct test_lifetime
{
  struct S : lifetime_dumper<S>
  {
  };
  void test_move_construct()
  {
    {
      var<int, S, float> c = S{};
      CHECK_THAT(c.get<S>().lifetime_info.copy_count) == 0;
      CHECK_THAT(c.get<S>().lifetime_info.move_count) == 1;
      c = S{};
      CHECK_THAT(c.get<S>().lifetime_info.copy_count) == 0;
      CHECK_THAT(c.get<S>().lifetime_info.move_count) == 1;
    }
    {
      var<int, S, float> c{S{}};
      CHECK_THAT(c.get<S>().lifetime_info.copy_count) == 0;
      CHECK_THAT(c.get<S>().lifetime_info.move_count) == 1;
    }
  }
  void test_copy_construct()
  {
    S                  s;
    var<int, S, float> c{s};
    CHECK_THAT(c.get<S>().lifetime_info.copy_count) == 1;
    CHECK_THAT(c.get<S>().lifetime_info.move_count) == 0;
    c = s;
    CHECK_THAT(c.get<S>().lifetime_info.copy_count) == 1;
    CHECK_THAT(c.get<S>().lifetime_info.move_count) == 0;
  }
};
} // namespace var_tests
