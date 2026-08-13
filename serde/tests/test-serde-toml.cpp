#include <doctest/doctest.h>

import reflex.serde.toml;

import std;

using namespace reflex;
using namespace reflex::serde;
using namespace std::literals;

// The bulk-scan cliff, pinned. An in-memory input is a contiguous cursor, a
// stream cursor is not.
static_assert(toml::deserializer<std::string_view::const_iterator>::bulk_scan);
static_assert(not toml::deserializer<std::istreambuf_iterator<char>>::bulk_scan);

// The registry is namespace reflection over ^^reflex::serde::ser, so a backend
// is only reachable from with_serializer() when the program *links* it, not
// merely when it imports it.
TEST_CASE("toml is registered as a serializer")
{
  std::string out;
  bool        seen = false;
  serde::with_serializer("toml", std::back_inserter(out), [&seen](auto& ser) {
    CHECK(std::remove_cvref_t<decltype(ser)>::format_name == "TOML"sv);
    seen = true;
  });
  CHECK(seen);
}

// Integer and Float are distinct TOML types, so toml::value carries both an
// int64_t and a double alternative. Two arithmetic alternatives is where a
// variant's converting constructor turns ambiguous, so the resolution is pinned
// here rather than assumed.
TEST_CASE("toml::value keeps an integer and a float apart")
{
  static_assert(toml::value::has_integral_type);
  static_assert(toml::value::has_floating_point_type);
  static_assert(std::same_as<toml::value::integral_type, std::int64_t>);
  static_assert(std::same_as<toml::value::floating_point_type, double>);

  const toml::value i = 9007199254740993LL; // 2^53 + 1, unrepresentable as a double
  const toml::value f = 1.5;
  const toml::value b = true;
  const toml::value s = "text";

  CHECK(i.is<toml::integer>());
  CHECK(i.as<toml::integer>() == 9007199254740993LL);
  CHECK(f.is<toml::number>());
  CHECK(f.as<toml::number>() == doctest::Approx(1.5));
  CHECK(b.is<toml::boolean>());
  CHECK(s.is<toml::string>());

  CHECK(std::format("{}", i) == "9007199254740993"sv);
  CHECK(reflex::visit([](auto const& v) { return std::format("{}", v); }, f) == "1.5"sv);

  toml::object obj;
  obj["n"] = 42;
  CHECK(obj["n"].is<toml::integer>());
}

TEST_CASE("toml is registered as a deserializer")
{
  bool found = false;
  template for(constexpr auto entry : define_static_array(serde::deserializers()))
  {
    if(identifier_of(entry) == "toml")
    {
      found = true;
    }
  }
  CHECK(found);
}
