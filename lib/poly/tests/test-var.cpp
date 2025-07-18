#include <reflex/var.hpp>

#include <exception>
#include <print>

#include <catch2/catch_test_macros.hpp>

using namespace reflex;

TEST_CASE("basic")
{
  using var_t = var<int, bool, std::string>;
  var_t c;
  REQUIRE(not c.has_value());
  STATIC_REQUIRE(c.can_hold<int, bool>());
  STATIC_REQUIRE(c.index_of<int>() == 0);
  STATIC_REQUIRE(c.index_of<bool>() == 1);
  STATIC_REQUIRE(c.index_of<double>() == -1);
  c = 42;
  REQUIRE(c.has_value());
  REQUIRE(c.has_value<int>());

  SECTION("changing value")
  {
    c = false;
    REQUIRE(c.has_value());
    REQUIRE(c.has_value<bool>());
    bool value = c;
    REQUIRE(value == false);
  }
  SECTION("string value")
  {
    using namespace std::string_literals;
    c = "hello"s;
    REQUIRE(c.has_value());
    REQUIRE(c.has_value<std::string>());
    std::println("{}", c);
    std::string value = c;
    REQUIRE(value == "hello");
  }
  SECTION("matching 1")
  {
    const auto r = c.match([](auto&& value) -> var_t {//
      return {value};
    });
    bool ok = r == c;
    REQUIRE(r == c);
    REQUIRE(r == 42);
    const int value = c;
    REQUIRE(value == 42);
    REQUIRE_THROWS_AS([&] { bool value = c; }(), bad_var_access);
  }
  SECTION("matching 2")
  {
    const auto r = c.match(
      [](int const& value) {//
        return true;
      },
      [](auto const& value) {//
        return false;
      });
    REQUIRE(r);
  }
  SECTION("matching 3")
  {
    REQUIRE(std::move(c).match(
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

TEST_CASE("lifetime")
{
  struct S : lifetime_dumper<S>
  {
  };
  SECTION("move-construct")
  {
    {
      var<int, S, float> c = S{};
      REQUIRE(c.get<S>().lifetime_info.copy_count == 0);
      REQUIRE(c.get<S>().lifetime_info.move_count == 1);
      c = S{};
      REQUIRE(c.get<S>().lifetime_info.copy_count == 0);
      REQUIRE(c.get<S>().lifetime_info.move_count == 1);
    }
    {
      var<int, S, float> c{S{}};
      REQUIRE(c.get<S>().lifetime_info.copy_count == 0);
      REQUIRE(c.get<S>().lifetime_info.move_count == 1);
    }
  }
  SECTION("copy-construct")
  {
    S                     s;
    var<int, S, float> c{s};
    REQUIRE(c.get<S>().lifetime_info.copy_count == 1);
    REQUIRE(c.get<S>().lifetime_info.move_count == 0);
    c = s;
    REQUIRE(c.get<S>().lifetime_info.copy_count == 1);
    REQUIRE(c.get<S>().lifetime_info.move_count == 0);
  }
}
