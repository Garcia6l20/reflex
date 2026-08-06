#include <doctest/doctest.h>

import reflex.serde.yaml;
import serde.tests.types;

import std;

using namespace reflex;
using namespace reflex::serde;
using namespace std::literals;

// The bulk-scan cliff, pinned. An in-memory input is a contiguous cursor, a
// stream cursor is not. The yaml parser is written to be correct on both; this
// is what makes the two runs of every later test case genuinely different code
// if a bulk fast path is ever added.
static_assert(yaml::deserializer<std::string_view::const_iterator>::bulk_scan);
static_assert(not yaml::deserializer<std::istreambuf_iterator<char>>::bulk_scan);

// The registry is namespace reflection over ^^reflex::serde::ser, so a backend
// is only reachable from with_serializer() when the program *links* it, not
// merely when it imports it. That distinction has bitten this repo before, so
// it is pinned rather than assumed.
TEST_CASE("yaml is registered as a serializer")
{
  bool found = false;
  template for(constexpr auto entry : define_static_array(serde::serializers()))
  {
    if(identifier_of(entry) == "yaml")
    {
      found = true;
    }
  }
  CHECK(found);
}

TEST_CASE("yaml is registered as a deserializer")
{
  bool found = false;
  template for(constexpr auto entry : define_static_array(serde::deserializers()))
  {
    if(identifier_of(entry) == "yaml")
    {
      found = true;
    }
  }
  CHECK(found);
}

template <typename T> static std::string dump(T const& value)
{
  std::string      out;
  yaml::serializer ser{out};
  ser.dump(value);
  return out;
}

TEST_CASE("reflex::serde::yaml: a plain scalar is written unquoted")
{
  CHECK(dump("hello"s) == "hello");
  CHECK(dump("hello world"s) == "hello world");
  CHECK(dump("a:b"s) == "a:b");     // no space after the colon
  CHECK(dump("a#b"s) == "a#b");     // no space before the hash
  CHECK(dump("/a/b"s) == "/a/b");
  CHECK(dump("1.2.3-rc1"s) == "1.2.3-rc1");
  CHECK(dump("-1a"s) == "-1a");     // '-' not followed by a space
  CHECK(dump("a,b"s) == "a,b");     // ',' is only structural in flow context
}

// Anything the core schema would resolve as a non-string has to come back out
// quoted, or it reads back as a different type.
TEST_CASE("reflex::serde::yaml: a scalar that would resolve as a non-string is quoted")
{
  CHECK(dump(""s) == "''");
  CHECK(dump("null"s) == "'null'");
  CHECK(dump("Null"s) == "'Null'");
  CHECK(dump("~"s) == "'~'");
  CHECK(dump("true"s) == "'true'");
  CHECK(dump("FALSE"s) == "'FALSE'");
  CHECK(dump("42"s) == "'42'");
  CHECK(dump("-7"s) == "'-7'");
  CHECK(dump("1.5"s) == "'1.5'");
  CHECK(dump("1e10"s) == "'1e10'");
  CHECK(dump(".inf"s) == "'.inf'");
  CHECK(dump(".nan"s) == "'.nan'");
  CHECK(dump("0x1F"s) == "'0x1F'");
  CHECK(dump("0o17"s) == "'0o17'");
  CHECK(dump("---"s) == "'---'");
  // YAML 1.2 reads these as strings, but YAML 1.1 does not. Quoted so the
  // document means the same thing to both.
  CHECK(dump("yes"s) == "'yes'");
  CHECK(dump("Off"s) == "'Off'");
  CHECK(dump("n"s) == "'n'");
}

TEST_CASE("reflex::serde::yaml: a scalar whose syntax bars a plain form is quoted")
{
  CHECK(dump(" "s) == "' '");
  CHECK(dump(" x"s) == "' x'");
  CHECK(dump("x "s) == "'x '");
  CHECK(dump("a: b"s) == "'a: b'");
  CHECK(dump("a #b"s) == "'a #b'");
  CHECK(dump("a:"s) == "'a:'");
  CHECK(dump("- x"s) == "'- x'");
  CHECK(dump("-"s) == "'-'");
  CHECK(dump("?"s) == "'?'");
  CHECK(dump(":"s) == "':'");
  CHECK(dump("#x"s) == "'#x'");
  CHECK(dump("[x"s) == "'[x'");
  CHECK(dump("&x"s) == "'&x'");
  CHECK(dump("*x"s) == "'*x'");
  CHECK(dump("%x"s) == "'%x'");
  CHECK(dump("|x"s) == "'|x'");
}

TEST_CASE("reflex::serde::yaml: single quotes are doubled, not backslashed")
{
  CHECK(dump("it's"s) == "it's"); // plain: an interior quote bars nothing
  CHECK(dump("'x"s) == "'''x'");  // leading quote is an indicator, so quoted
  CHECK(dump("' "s) == "''' '");
}

// Single-quoted style holds any printable byte, so the double-quoted form is
// reached exactly when a control character or a tab is present.
TEST_CASE("reflex::serde::yaml: a control character forces double quotes")
{
  CHECK(dump("line\nbreak"s) == "\"line\\nbreak\"");
  CHECK(dump("a\tb"s) == "\"a\\tb\"");
  CHECK(dump("a\x01"s) == "\"a\\x01\"");
  CHECK(dump("a\x7F"s) == "\"a\\x7f\"");
  CHECK(dump("a\rb"s) == "\"a\\rb\"");
  CHECK(dump("a\x1B[0m"s) == "\"a\\e[0m\"");
  CHECK(dump("a\"b\nc"s) == "\"a\\\"b\\nc\"");
  CHECK(dump("a\\b\nc"s) == "\"a\\\\b\\nc\"");
}

TEST_CASE("reflex::serde::yaml: UTF-8 passes through a plain scalar unchanged")
{
  CHECK(dump("héllo"s) == "héllo");
  CHECK(dump("日本"s) == "日本");
}

TEST_CASE("reflex::serde::yaml: numbers")
{
  CHECK(dump(42) == "42");
  CHECK(dump(-7) == "-7");
  CHECK(dump(1.5) == "1.5");
  CHECK(dump(0.0) == "0");
  CHECK(dump(-0.0) == "-0");
  CHECK(dump(std::numeric_limits<double>::infinity()) == ".inf");
  CHECK(dump(-std::numeric_limits<double>::infinity()) == "-.inf");
  CHECK(dump(std::numeric_limits<double>::quiet_NaN()) == ".nan");
}

TEST_CASE("reflex::serde::yaml: booleans, chars and null")
{
  CHECK(dump(true) == "true");
  CHECK(dump(false) == "false");
  CHECK(dump('a') == "a");
  CHECK(dump(':') == "':'");
  CHECK(dump('\n') == "\"\\n\"");
  CHECK(dump(yaml::null) == "null");
}

TEST_CASE("reflex::serde::yaml: optional")
{
  CHECK(dump(std::optional<int>{3}) == "3");
  CHECK(dump(std::optional<int>{}) == "null");
  CHECK(dump(std::optional<std::string>{"null"s}) == "'null'");
}

TEST_CASE("reflex::serde::yaml: fixed-capacity string targets")
{
  std::array<char, 8> arr{};
  arr[0] = 'a';
  arr[1] = 'b';
  CHECK(dump(arr) == "ab"); // trimmed at the first NUL, not padded
  CHECK(dump(heapless::string<8>{"ab"}) == "ab");
}

TEST_CASE("reflex::serde::yaml: a Format-derived enum renders through its formatter")
{
  CHECK(dump(Color::Green) == "Green");
}
