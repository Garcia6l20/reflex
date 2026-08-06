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

struct[[= serde::naming::camel_case, = derive(Debug)]] Basic
{
  int                                    int_member;
  std::string                            string_member;
  [[= serde::naming::kebab_case]] double double_member;

  constexpr bool operator==(Basic const& other) const = default;
};

struct[[= derive(Debug)]] Inner
{
  int         a;
  std::string b;

  constexpr bool operator==(Inner const& other) const = default;
};

struct[[= derive(Debug)]] Outer
{
  int   before;
  Inner inner;
  int   after;

  constexpr bool operator==(Outer const& other) const = default;
};

struct[[= derive(Debug)]] Deep
{
  Outer outer;

  constexpr bool operator==(Deep const& other) const = default;
};

struct Empty
{
  constexpr bool operator==(Empty const&) const = default;
};

struct[[= derive(Debug)]] WithEmpty
{
  int   a;
  Empty e;
  int   b;

  constexpr bool operator==(WithEmpty const& other) const = default;
};

struct[[= derive(Debug)]] Renamed
{
  [[= serde::rename{"a: b"}]] int awkward;
  [[= serde::rename{"it's"}]] int quoted;

  constexpr bool operator==(Renamed const& other) const = default;
};

struct[[= derive(Debug)]] OptAggregate
{
  std::optional<Inner> inner;
  int                  after;

  constexpr bool operator==(OptAggregate const& other) const = default;
};

TEST_CASE("reflex::serde::yaml: a flat aggregate is a block mapping with no trailing newline")
{
  CHECK(dump(Inner{1, "two"}) == "a: 1\nb: two");
}

TEST_CASE("reflex::serde::yaml: member names go through serialized_name")
{
  CHECK(dump(Basic{1, "x", 2.5}) == "intMember: 1\nstringMember: x\ndouble-member: 2.5");
}

TEST_CASE("reflex::serde::yaml: a nested aggregate opens an indented block")
{
  CHECK(dump(Outer{1, {2, "two"}, 3}) == "before: 1\ninner:\n  a: 2\n  b: two\nafter: 3");
}

TEST_CASE("reflex::serde::yaml: nesting indents by two columns per level")
{
  CHECK(
      dump(Deep{{1, {2, "two"}, 3}})
      == "outer:\n  before: 1\n  inner:\n    a: 2\n    b: two\n  after: 3");
}

TEST_CASE("reflex::serde::yaml: an aggregate with no members is {}")
{
  CHECK(dump(Empty{}) == "{}");
  CHECK(dump(WithEmpty{1, {}, 2}) == "a: 1\ne: {}\nb: 2");
}

TEST_CASE("reflex::serde::yaml: a renamed key is quoted when a plain form would not read back")
{
  CHECK(dump(Renamed{1, 2}) == "'a: b': 1\nit's: 2");
}

TEST_CASE("reflex::serde::yaml: an optional member")
{
  CHECK(dump(Opt{"x", 3}) == "name: x\ncount: 3");
  CHECK(dump(Opt{"x", std::nullopt}) == "name: x\ncount: null");
}

// std::optional is not itself a block node, so the separator decision has to
// see through it or an engaged optional of an aggregate starts on the key's
// line and produces a broken document.
TEST_CASE("reflex::serde::yaml: an optional aggregate member still opens a block")
{
  CHECK(dump(OptAggregate{Inner{1, "two"}, 3}) == "inner:\n  a: 1\n  b: two\nafter: 3");
  CHECK(dump(OptAggregate{std::nullopt, 3}) == "inner: null\nafter: 3");
}

struct[[= derive(Debug)]] WithSeq
{
  std::string      name;
  std::vector<int> values;

  constexpr bool operator==(WithSeq const& other) const = default;
};

struct[[= derive(Debug)]] WithMap
{
  std::string                        name;
  std::map<std::string, int>         m;

  bool operator==(WithMap const& other) const = default;
};

TEST_CASE("reflex::serde::yaml: a sequence is one '- ' per element")
{
  CHECK(dump(std::vector<int>{1, 2, 3}) == "- 1\n- 2\n- 3");
  CHECK(dump(std::vector<int>{}) == "[]");
  CHECK(dump(std::vector<std::string>{"a", "null"}) == "- a\n- 'null'");
}

TEST_CASE("reflex::serde::yaml: a sequence member is indented under its key")
{
  CHECK(dump(WithSeq{"x", {1, 2}}) == "name: x\nvalues:\n  - 1\n  - 2");
  CHECK(dump(WithSeq{"x", {}}) == "name: x\nvalues: []");
}

// The compact notation: a block child starts right after the "- ", and its
// continuation lines line up under it. No special case produces this - it is
// what the indent invariant already does.
TEST_CASE("reflex::serde::yaml: a sequence of sequences uses the compact notation")
{
  CHECK(dump(std::vector<std::vector<int>>{{1, 2}, {3}}) == "- - 1\n  - 2\n- - 3");
  CHECK(dump(std::vector<std::vector<int>>{{}, {1}}) == "- []\n- - 1");
}

TEST_CASE("reflex::serde::yaml: a sequence of mappings uses the compact notation")
{
  CHECK(
      dump(std::vector<Inner>{{1, "two"}, {2, "three"}})
      == "- a: 1\n  b: two\n- a: 2\n  b: three");
}

TEST_CASE("reflex::serde::yaml: three levels of nesting")
{
  // "y" comes out quoted: it is a YAML 1.1 boolean, and step 02 quotes those on
  // purpose so the document reads the same to a 1.1 parser. Kept as the fixture
  // value rather than swapped for a neutral one, since it is worth seeing the
  // rule fire somewhere other than its own test.
  CHECK(
      dump(std::vector<WithSeq>{{"x", {1, 2}}, {"y", {}}})
      == "- name: x\n  values:\n    - 1\n    - 2\n- name: 'y'\n  values: []");
}

TEST_CASE("reflex::serde::yaml: a std::array of scalars is a sequence, not an aggregate")
{
  CHECK(dump(std::array<int, 3>{1, 2, 3}) == "- 1\n- 2\n- 3");
}

// A std::array<char, N> satisfies both str_c and seq_c. It is a scalar.
TEST_CASE("reflex::serde::yaml: a char array inside a sequence stays a scalar")
{
  std::array<char, 4> a{};
  a[0] = 'a';
  a[1] = 'b';
  CHECK(dump(std::vector<std::array<char, 4>>{a}) == "- ab");
}

TEST_CASE("reflex::serde::yaml: mappings with runtime keys")
{
  CHECK(dump(std::map<std::string, int>{{"a", 1}, {"b", 2}}) == "a: 1\nb: 2");
  CHECK(dump(std::map<std::string, int>{}) == "{}");
  CHECK(dump(WithMap{"x", {{"a", 1}}}) == "name: x\nm:\n  a: 1");
  CHECK(dump(WithMap{"x", {}}) == "name: x\nm: {}");
}

// An int key stays plain so it resolves back to an int; a string key that would
// resolve as something else gets quoted.
TEST_CASE("reflex::serde::yaml: a mapping key is serialized, not re-quoted")
{
  CHECK(dump(std::map<int, std::string>{{1, "one"}, {2, "two"}}) == "1: one\n2: two");
  CHECK(dump(std::map<std::string, int>{{"a: b", 1}}) == "'a: b': 1");
  CHECK(dump(std::map<std::string, int>{{"1", 1}}) == "'1': 1");
}

TEST_CASE("reflex::serde::yaml: a mapping of composite values")
{
  CHECK(dump(std::map<std::string, Inner>{{"k", {1, "two"}}}) == "k:\n  a: 1\n  b: two");
  CHECK(
      dump(std::map<std::string, std::vector<int>>{{"k", {1, 2}}}) == "k:\n  - 1\n  - 2");
}

TEST_CASE("reflex::serde::yaml: a standalone pair is a one-entry mapping")
{
  CHECK(dump(std::pair<std::string, int>{"a", 1}) == "a: 1");
  CHECK(dump(std::vector<std::pair<std::string, int>>{{"a", 1}, {"b", 2}}) == "- a: 1\n- b: 2");
}

TEST_CASE("reflex::serde::yaml: poly::var")
{
  using var = yaml::value;

  CHECK(dump(var{}) == "null");
  CHECK(dump(var{true}) == "true");
  CHECK(dump(var{1.5}) == "1.5");
  CHECK(dump(var{"x"s}) == "x");
  CHECK(dump(var{"true"s}) == "'true'");

  yaml::array arr;
  arr.push_back(var{1.0});
  arr.push_back(var{"x"s});
  CHECK(dump(var{arr}) == "- 1\n- x");

  yaml::object obj;
  obj.emplace("a", var{1.0});
  CHECK(dump(var{obj}) == "a: 1");
}

TEST_CASE("reflex::serde::yaml: a poly::var member opens a block when it holds a collection")
{
  struct[[= derive(Debug)]] Holder
  {
    yaml::value v;
    int         after;
  };

  yaml::array arr;
  arr.push_back(yaml::value{1.0});
  arr.push_back(yaml::value{2.0});
  CHECK(dump(Holder{yaml::value{arr}, 3}) == "v:\n  - 1\n  - 2\nafter: 3");
  CHECK(dump(Holder{yaml::value{"x"s}, 3}) == "v: x\nafter: 3");
  CHECK(dump(Holder{yaml::value{}, 3}) == "v: null\nafter: 3");
}

// The depth counter must return to zero after every nested value, or the next
// sibling is written one level too deep. The unwind-through-a-throw half of the
// guard's contract is untested: the yaml serializer has no runtime throw path
// at all - YAML has a literal for every value the other backends reject, and a
// heapless::string overflow is a compile-time assert, not an exception.
TEST_CASE("reflex::serde::yaml: the depth counter returns to zero after a nested value")
{
  CHECK(dump(Outer{1, {2, "two"}, 3}).ends_with("\nafter: 3"));

  std::string      out;
  yaml::serializer ser{out};
  ser.dump(Deep{{1, {2, "two"}, 3}});
  out.clear();
  ser.dump(Inner{1, "two"});
  CHECK(out == "a: 1\nb: two"); // same serializer, depth back at 0
}
