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
    check_that(not c.has_value());
    check_that(c.can_hold<int, bool>());
    check_that(c.index_of<none_t>()) == 0;
    check_that(c.index_of<int>()) == 1;
    check_that(c.index_of<bool>()) == 2;
    check_that(c.index_of<double>()) == -1;
    c = 42;
    check_that(c.has_value());
    check_that(c.has_value<int>());
  }

  void test_changing_value()
  {
    c = false;
    check_that(c.has_value());
    check_that(c.has_value<bool>());
    bool value = c;
    check_that(value) == false;
  }
  void test_string_value()
  {
    c = "hello";
    check_that(c.has_value());
    check_that(c.has_value<std::string>());
    std::println("{}", c);
    std::string value = c;
    check_that(value) == "hello";
  }
  void test_matching_1()
  {
    const auto r = c.match([](auto&& value) -> var_t {//
      return {value};
    });
    check_that(r) == expr(c);
    c = 55;
    check_that(r) != expr(c);
    check_that(r) == 42;
    const int value = r;
    check_that(value) == 42;
    bool bvalue;
    eval_check_that(bvalue = c).throws<bad_var_access>();
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
    check_that(r);
  }
  void test_matching_3()
  {
    check_that(std::move(c).match(
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
  static_check_that(not meta::has_explicit_constructor(^^int,
                                                       {
                                                           ^^const char* }));
  static_check_that(not meta::has_explicit_constructor(^^bool,
                                                       {
                                                           ^^const char* }));
  static_check_that(meta::has_explicit_constructor(^^std::string,
                                                   {
                                                       ^^const char* }));

  static_check_that(not std::equality_comparable_with<int, const char*>);
  static_check_that(not std::equality_comparable_with<bool, const char*>);
  static_check_that(std::equality_comparable_with<std::string, const char*>);
  static_check_that(var_t::equality_comparable_with<const char*>());
  // construct
  {
    var_t c{"hello constructed"};
    std::println("{}", c);
    check_that(c) == "hello constructed";
  }
  // assign
  {
    var_t c = "hello assigned";
    std::println("{}", c);
    check_that(c) == "hello assigned";
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
      check_that(c.get<S>().lifetime_info.copy_count) == 0;
      check_that(c.get<S>().lifetime_info.move_count) == 1;
      c = S{};
      check_that(c.get<S>().lifetime_info.copy_count) == 0;
      check_that(c.get<S>().lifetime_info.move_count) == 1;
    }
    {
      var<int, S, float> c{S{}};
      check_that(c.get<S>().lifetime_info.copy_count) == 0;
      check_that(c.get<S>().lifetime_info.move_count) == 1;
    }
  }
  void test_copy_construct()
  {
    S                  s;
    var<int, S, float> c{s};
    check_that(c.get<S>().lifetime_info.copy_count) == 1;
    check_that(c.get<S>().lifetime_info.move_count) == 0;
    c = s;
    check_that(c.get<S>().lifetime_info.copy_count) == 1;
    check_that(c.get<S>().lifetime_info.move_count) == 0;
  }
};
} // namespace var_tests
