#include <doctest/doctest.h>

import reflex.serde.yaml;
// For the superset check: a document the json backend produced has to parse
// here, and generating it beats hand-writing one that only looks like json.
import reflex.serde.json;
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

namespace
{
  // Every cursor case runs twice: once over a contiguous buffer, once through a
  // stream cursor that has bulk_scan off. The parser is one implementation, so
  // the two must agree exactly - and if a bulk fast path is ever added, this is
  // what proves it equivalent rather than merely faster.
  template <typename Fn> void both_cursors(std::string_view text, Fn&& fn)
  {
    {
      yaml::deserializer de{text};
      fn(de);
    }
    {
      std::istringstream in{std::string{text}};
      yaml::deserializer de{in};
      fn(de);
    }
  }
} // namespace

TEST_CASE("reflex::serde::yaml::deserializer: lines and columns")
{
  both_cursors("ab\ncd", [](auto& de) {
    CHECK(de.column() == 0);
    CHECK(de.advance() == 'a');
    CHECK(de.column() == 1);
    CHECK(de.advance() == 'b');
    CHECK(de.at_line_end());
    CHECK(de.next_line());
    CHECK(de.column() == 0);
    CHECK(de.peek() == 'c');
  });
}

// advance() refuses a line break: a reader that runs past a line end has lost
// the column, and every column after it would be wrong.
TEST_CASE("reflex::serde::yaml::deserializer: advance refuses a line break")
{
  both_cursors("a\nb", [](auto& de) {
    CHECK(de.advance() == 'a');
    CHECK_THROWS(de.advance());
  });
}

TEST_CASE("reflex::serde::yaml::deserializer: CRLF is one break")
{
  both_cursors("ab\r\ncd", [](auto& de) {
    de.advance();
    de.advance();
    CHECK(de.at_line_end());
    CHECK(de.next_line());
    CHECK(de.column() == 0);
    CHECK(de.peek() == 'c');
  });
}

TEST_CASE("reflex::serde::yaml::deserializer: end of input without a trailing break")
{
  both_cursors("a", [](auto& de) {
    CHECK(de.advance() == 'a');
    CHECK(de.at_end());
    CHECK(not de.next_line());
  });
}

TEST_CASE("reflex::serde::yaml::deserializer: indentation")
{
  both_cursors("  a", [](auto& de) {
    CHECK(de.skip_indent() == 2);
    CHECK(de.column() == 2);
    CHECK(de.peek() == 'a');
  });
  both_cursors("\ta", [](auto& de) { CHECK_THROWS(de.skip_indent()); });
  // A tab on a line carrying no node is not an indentation error.
  both_cursors("\t\na", [](auto& de) { CHECK_NOTHROW(de.skip_indent()); });
  both_cursors("\t# c\na", [](auto& de) { CHECK_NOTHROW(de.skip_indent()); });
}

TEST_CASE("reflex::serde::yaml::deserializer: next_content_line skips blanks and comments")
{
  both_cursors("\n\n  # c\n  a", [](auto& de) {
    CHECK(de.next_content_line() == 2);
    CHECK(de.peek() == 'a');
  });
  both_cursors("a", [](auto& de) {
    CHECK(de.next_content_line() == 0);
    CHECK(de.peek() == 'a');
  });
  both_cursors("# only a comment", [](auto& de) { CHECK(de.next_content_line() == de.npos); });
  both_cursors("", [](auto& de) { CHECK(de.next_content_line() == de.npos); });
}

// A nested block stops on the first line it does not own, and its parent then
// asks about that same line. Without idempotence the parent skips a key.
TEST_CASE("reflex::serde::yaml::deserializer: next_content_line is idempotent on a node")
{
  both_cursors("  a\n  b", [](auto& de) {
    CHECK(de.next_content_line() == 2);
    CHECK(de.next_content_line() == 2);
    CHECK(de.peek() == 'a');
    de.advance();
    CHECK(de.next_content_line() == 2);
    CHECK(de.peek() == 'b');
  });
}

TEST_CASE("reflex::serde::yaml::deserializer: a comment needs whitespace before it")
{
  both_cursors("a # c\nb", [](auto& de) {
    CHECK(de.advance() == 'a');
    CHECK(de.next_content_line() == 0);
    CHECK(de.peek() == 'b');
  });
  // '#' with a non-space before it is scalar content, not a comment.
  both_cursors("a#b\nc", [](auto& de) {
    CHECK(de.advance() == 'a');
    CHECK(de.peek() == '#');
  });
}

TEST_CASE("reflex::serde::yaml::deserializer: trailing content after a value is an error")
{
  both_cursors("a b\nc", [](auto& de) {
    CHECK(de.advance() == 'a');
    CHECK_THROWS(de.finish_line());
  });
}

TEST_CASE("reflex::serde::yaml::deserializer: document markers")
{
  both_cursors("---\na: 1", [](auto& de) {
    CHECK(de.next_content_line() == 0);
    CHECK(de.peek() == 'a');
  });
  both_cursors("a: 1\n...\nb", [](auto& de) {
    CHECK(de.next_content_line() == 0);
    de.skip_to_line_end();
    CHECK(de.next_content_line() == de.npos);
  });
  both_cursors("---\na: 1\n---\nb: 2", [](auto& de) {
    CHECK(de.next_content_line() == 0);
    de.skip_to_line_end();
    CHECK_THROWS(de.next_content_line());
  });
  both_cursors("%YAML 1.2\na: 1", [](auto& de) { CHECK_THROWS(de.next_content_line()); });
  // "---" only marks a document at column 0 and as a whole token.
  both_cursors("---foo", [](auto& de) {
    CHECK(de.next_content_line() == 0);
    CHECK(de.peek() == '-');
  });
}

namespace
{
  // Both cursors, one answer. Every load case below runs through this.
  template <typename T> void check_load(std::string_view text, T const& expected)
  {
    CHECK(yaml::deserializer{text}.template load<T>() == expected);
    std::istringstream in{std::string{text}};
    CHECK(yaml::deserializer{in}.template load<T>() == expected);
  }

  template <typename T> void check_load_throws(std::string_view text)
  {
    CHECK_THROWS(yaml::deserializer{text}.template load<T>());
    std::istringstream in{std::string{text}};
    CHECK_THROWS(yaml::deserializer{in}.template load<T>());
  }
} // namespace

TEST_CASE("reflex::serde::yaml::deserializer: plain scalars")
{
  check_load<std::string>("hello", "hello");
  check_load<std::string>("hello world", "hello world");
  check_load<std::string>("a:b", "a:b");
  check_load<std::string>("a#b", "a#b");
  check_load<std::string>("http://x#y", "http://x#y");
  check_load<std::string>("1.2.3-rc1", "1.2.3-rc1");
  check_load<std::string>("a,b", "a,b"); // ',' is only structural in flow context
  // Trailing whitespace is not part of a plain scalar, interior space is.
  check_load<std::string>("hello   ", "hello");
  check_load<std::string>("a  b", "a  b");
  check_load<std::string>("a #c", "a");
}

TEST_CASE("reflex::serde::yaml::deserializer: quoted scalars")
{
  check_load<std::string>("'it''s'", "it's");
  check_load<std::string>("''", "");
  check_load<std::string>("'a: b'", "a: b");
  check_load<std::string>("'null'", "null");
  check_load<std::string>("\"a\\nb\"", "a\nb");
  check_load<std::string>("\"a\\tb\"", "a\tb");
  check_load<std::string>("\"\\x41\"", "A");
  check_load<std::string>("\"\\u0041\"", "A");
  check_load<std::string>("\"\\e[0m\"", "\x1B[0m");
  check_load<std::string>("\"a\\\"b\"", "a\"b");
  check_load<std::string>("\"a\\\\b\"", "a\\b");
  check_load_throws<std::string>("'unterminated");
  check_load_throws<std::string>("\"unterminated");
  check_load_throws<std::string>("\"\\q\"");
  check_load_throws<std::string>("\"\\u00e9\""); // above 0x7F, not implemented
}

TEST_CASE("reflex::serde::yaml::deserializer: literal block scalars")
{
  check_load<std::string>("|\n  one\n  two", "one\ntwo\n");
  check_load<std::string>("|-\n  one\n  two", "one\ntwo");
  check_load<std::string>("|+\n  one\n  two\n\n", "one\ntwo\n\n");
  check_load<std::string>("|\n  one\n\n  two", "one\n\ntwo\n");
  check_load<std::string>("|\n  one\n    deeper", "one\n  deeper\n");
  check_load<std::string>("|2\n  one\n  two", "one\ntwo\n");
  check_load<std::string>("|\n  # not a comment", "# not a comment\n");
}

TEST_CASE("reflex::serde::yaml::deserializer: folded block scalars")
{
  check_load<std::string>(">\n  one\n  two", "one two\n");
  check_load<std::string>(">-\n  one\n  two", "one two");
  check_load<std::string>(">\n  one\n\n  two", "one\ntwo\n");
  check_load<std::string>(">\n  one", "one\n");
}

TEST_CASE("reflex::serde::yaml::deserializer: numbers")
{
  check_load<int>("42", 42);
  check_load<int>("-7", -7);
  check_load<int>("+7", 7);
  check_load<int>("0x1F", 31);
  check_load<int>("0o17", 15);
  check_load<double>("1.5", 1.5);
  check_load<double>("-0.5", -0.5);
  check_load<double>("1e10", 1e10);
  check_load<double>(".inf", std::numeric_limits<double>::infinity());
  check_load<double>("-.inf", -std::numeric_limits<double>::infinity());
  CHECK(std::isnan(yaml::deserializer{".nan"sv}.load<double>()));

  check_load_throws<int>("1.5");
  check_load_throws<int>("42abc");
  check_load_throws<double>("1.2.3");
  check_load_throws<unsigned>("-1");
}

TEST_CASE("reflex::serde::yaml::deserializer: booleans")
{
  check_load<bool>("true", true);
  check_load<bool>("True", true);
  check_load<bool>("TRUE", true);
  check_load<bool>("false", false);
  check_load<bool>("False", false);
  // A 1.1 boolean is a string in 1.2, so guessing would make the value depend
  // on the reader. Named error instead.
  check_load_throws<bool>("yes");
  check_load_throws<bool>("off");
  check_load_throws<bool>("y");
  check_load_throws<bool>("maybe");
}

TEST_CASE("reflex::serde::yaml::deserializer: null and optional")
{
  check_load<std::optional<int>>("3", std::optional<int>{3});
  check_load<std::optional<int>>("null", std::nullopt);
  check_load<std::optional<int>>("Null", std::nullopt);
  check_load<std::optional<int>>("NULL", std::nullopt);
  check_load<std::optional<int>>("~", std::nullopt);
  check_load<std::optional<int>>("", std::nullopt);
  // "nullify" starts with "null" but is not one.
  check_load<std::optional<std::string>>("nullify", std::optional<std::string>{"nullify"});
}

TEST_CASE("reflex::serde::yaml::deserializer: string sinks")
{
  check_load<reflex::heapless::string<8>>("ab", reflex::heapless::string<8>{"ab"});
  check_load_throws<reflex::heapless::string<2>>("abcdef");

  std::array<char, 4> expected{};
  expected[0] = 'a';
  expected[1] = 'b';
  check_load<std::array<char, 4>>("ab", expected);
  check_load_throws<std::array<char, 2>>("abcdef");
}

TEST_CASE("reflex::serde::yaml::deserializer: a Parse-derived type")
{
  check_load<Color>("Green", Color::Green);
  check_load_throws<Color>("Mauve");
}

// Whether a plain scalar needs quoting on the way out and whether it survives a
// plain read on the way back are two halves of one rule. This is what keeps
// them from drifting.
TEST_CASE("reflex::serde::yaml: an unquoted scalar reads back as itself")
{
  static constexpr std::string_view plain[] = {
      "hello", "hello world", "a:b", "a#b", "/a/b", "1.2.3-rc1", "-1a", "a,b", "it's", "héllo"};
  for(std::string_view s : plain)
  {
    CAPTURE(s);
    CHECK(dump(std::string{s}) == s); // the writer leaves it plain
    check_load<std::string>(s, std::string{s});
  }
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

TEST_CASE("reflex::serde::yaml::deserializer: a flat block mapping")
{
  check_load<Inner>("a: 1\nb: two", Inner{1, "two"});
  check_load<Inner>("a: 1\nb: two\n", Inner{1, "two"}); // trailing newline
  check_load<Basic>(
      "intMember: 1\nstringMember: x\ndouble-member: 2.5", Basic{1, "x", 2.5});
}

TEST_CASE("reflex::serde::yaml::deserializer: nested block mappings")
{
  check_load<Outer>("before: 1\ninner:\n  a: 2\n  b: two\nafter: 3", Outer{1, {2, "two"}, 3});
  check_load<Deep>(
      "outer:\n  before: 1\n  inner:\n    a: 2\n    b: two\n  after: 3",
      Deep{{1, {2, "two"}, 3}});
}

// The key after a nested block is the one next_content_line()'s idempotence
// exists for: the nested reader stops on that line, and this loop must not
// skip it.
TEST_CASE("reflex::serde::yaml::deserializer: a key following a nested block is not skipped")
{
  check_load<Outer>("inner:\n  a: 2\n  b: two\nbefore: 1\nafter: 3", Outer{1, {2, "two"}, 3});
}

TEST_CASE("reflex::serde::yaml::deserializer: quoted keys")
{
  check_load<Renamed>("'a: b': 1\nit's: 2", Renamed{1, 2});
  check_load<Renamed>("\"a: b\": 1\nit's: 2", Renamed{1, 2});
}

TEST_CASE("reflex::serde::yaml::deserializer: comments and blank lines between entries")
{
  check_load<Inner>("# leading\na: 1\n\n# between\nb: two\n# trailing", Inner{1, "two"});
  check_load<Inner>("a: 1 # trailing on the line\nb: two", Inner{1, "two"});
}

TEST_CASE("reflex::serde::yaml::deserializer: a missing key leaves the member default")
{
  check_load<Inner>("a: 1", Inner{1, ""});
}

TEST_CASE("reflex::serde::yaml::deserializer: an empty value is a null")
{
  check_load<Opt>("name: x\ncount:", Opt{"x", std::nullopt});
  check_load<Opt>("name: x\ncount: null", Opt{"x", std::nullopt});
  check_load<Inner>("a: 1\nb:", Inner{1, ""});
}

TEST_CASE("reflex::serde::yaml::deserializer: an optional aggregate member")
{
  check_load<OptAggregate>("inner:\n  a: 1\n  b: two\nafter: 3", OptAggregate{Inner{1, "two"}, 3});
  check_load<OptAggregate>("inner: null\nafter: 3", OptAggregate{std::nullopt, 3});
}

TEST_CASE("reflex::serde::yaml::deserializer: a block scalar as a mapping value")
{
  check_load<Inner>("a: 1\nb: |\n  one\n  two", Inner{1, "one\ntwo\n"});
  check_load<Outer>("before: 1\ninner:\n  a: 2\n  b: |-\n    x\nafter: 3", Outer{1, {2, "x"}, 3});
}

TEST_CASE("reflex::serde::yaml::deserializer: malformed mappings are named errors")
{
  check_load_throws<Inner>("a: 1\n  b: two");  // over-indented continuation
  check_load_throws<Inner>("a: 1\nnosuchkey: 2"); // object_visitor rejects an unknown key
  check_load_throws<Inner>("a 1");             // no colon
}

// An unknown key is an error, not a skip. That is object_visitor's behaviour
// (object_visit.hpp:132), shared with every other backend, not a yaml choice.
TEST_CASE("reflex::serde::yaml::deserializer: an unknown key is rejected")
{
  check_load_throws<Inner>("a: 1\nb: two\nc: 3");
}

TEST_CASE("reflex::serde::yaml::deserializer: a block sequence")
{
  check_load<std::vector<int>>("- 1\n- 2\n- 3", std::vector<int>{1, 2, 3});
  check_load<std::vector<std::string>>("- a\n- 'null'", std::vector<std::string>{"a", "null"});
  check_load<std::vector<int>>("- 1\n- 2\n", std::vector<int>{1, 2}); // trailing newline
}

// Both spellings are the same document. The serializer emits the indented one;
// the flush one is what hand-written YAML overwhelmingly uses.
TEST_CASE("reflex::serde::yaml::deserializer: a sequence under a key, indented or flush")
{
  check_load<WithSeq>("name: x\nvalues:\n  - 1\n  - 2", WithSeq{"x", {1, 2}});
  check_load<WithSeq>("name: x\nvalues:\n- 1\n- 2", WithSeq{"x", {1, 2}});
  // The flush form again, with a key after it: the sequence and the parent
  // mapping share an indent, so only "this line is not an entry" ends it.
  check_load<WithSeq>("values:\n- 1\n- 2\nname: x", WithSeq{"x", {1, 2}});
}

TEST_CASE("reflex::serde::yaml::deserializer: the compact notation")
{
  check_load<std::vector<std::vector<int>>>(
      "- - 1\n  - 2\n- - 3", std::vector<std::vector<int>>{{1, 2}, {3}});
  check_load<std::vector<Inner>>(
      "- a: 1\n  b: two\n- a: 2\n  b: three", std::vector<Inner>{{1, "two"}, {2, "three"}});
  check_load<std::vector<WithSeq>>(
      "- name: x\n  values:\n    - 1\n    - 2\n- name: 'y'\n  values:\n    - 3",
      std::vector<WithSeq>{{"x", {1, 2}}, {"y", {3}}});
  check_load<std::vector<WithSeq>>(
      "- name: x\n  values:\n    - 1\n    - 2\n- name: 'y'\n  values: []",
      std::vector<WithSeq>{{"x", {1, 2}}, {"y", {}}});
}

// "- 1" is an entry and "-1" is a number. The space is the whole difference.
TEST_CASE("reflex::serde::yaml::deserializer: a dash without a space is not an entry")
{
  check_load<std::vector<int>>("- -1\n- -2", std::vector<int>{-1, -2});
  check_load<int>("-1", -1);
}

TEST_CASE("reflex::serde::yaml::deserializer: an entry with nothing after the dash is null")
{
  check_load<std::vector<std::optional<int>>>(
      "-\n-", std::vector<std::optional<int>>{std::nullopt, std::nullopt});
  check_load<std::vector<int>>("-\n- 2", std::vector<int>{0, 2});
}

TEST_CASE("reflex::serde::yaml::deserializer: an entry whose value is on the following lines")
{
  check_load<std::vector<Inner>>("-\n  a: 1\n  b: two", std::vector<Inner>{{1, "two"}});
}

TEST_CASE("reflex::serde::yaml::deserializer: a fixed-capacity sequence destination")
{
  check_load<std::array<int, 3>>("- 1\n- 2\n- 3", std::array<int, 3>{1, 2, 3});
  check_load_throws<std::array<int, 2>>("- 1\n- 2\n- 3");
}

TEST_CASE("reflex::serde::yaml::deserializer: malformed sequences are named errors")
{
  check_load_throws<std::vector<int>>("- 1\n  - 2"); // over-indented entry
  check_load_throws<std::vector<int>>("a: 1");       // a mapping where a sequence was asked for
}

TEST_CASE("reflex::serde::yaml::deserializer: flow sequences")
{
  check_load<std::vector<int>>("[1, 2, 3]", std::vector<int>{1, 2, 3});
  check_load<std::vector<int>>("[]", std::vector<int>{});
  check_load<std::vector<int>>("[1, 2, ]", std::vector<int>{1, 2}); // legal in YAML, not in JSON
  check_load<std::vector<std::string>>("[a, b]", std::vector<std::string>{"a", "b"});
  // The flow terminator set applies here and only here: in block context this
  // would be one scalar running to the end of the line.
  check_load<std::vector<std::string>>("[1.2.3-rc1]", std::vector<std::string>{"1.2.3-rc1"});
  check_load<std::vector<std::vector<int>>>(
      "[[1, 2], [3]]", std::vector<std::vector<int>>{{1, 2}, {3}});
  check_load_throws<std::vector<int>>("[1, 2");
}

TEST_CASE("reflex::serde::yaml::deserializer: flow mappings")
{
  check_load<Inner>("{a: 1, b: two}", Inner{1, "two"});
  check_load<Inner>("{a: 1, b: two, }", Inner{1, "two"});
  check_load<Empty>("{}", Empty{});
  check_load<Outer>("{before: 1, inner: {a: 2, b: two}, after: 3}", Outer{1, {2, "two"}, 3});
  check_load_throws<Inner>("{a: 1");
}

// A flow collection may span lines, and a break inside one is just whitespace.
TEST_CASE("reflex::serde::yaml::deserializer: a flow collection spanning lines")
{
  check_load<std::vector<int>>("[\n  1,\n  2\n]", std::vector<int>{1, 2});
  check_load<Inner>("{\n  a: 1,\n  b: two\n}", Inner{1, "two"});
  check_load<std::vector<int>>("[ # a comment\n  1,\n  2\n]", std::vector<int>{1, 2});
}

// Entering and leaving flow has to restore the enclosing block indent, or the
// block key after the flow value inherits a neutralised one.
TEST_CASE("reflex::serde::yaml::deserializer: flow nested inside block")
{
  check_load<WithSeq>("name: x\nvalues: [1, 2]", WithSeq{"x", {1, 2}});
  check_load<WithSeq>("values: [1, 2]\nname: x", WithSeq{"x", {1, 2}});
  check_load<Outer>("before: 1\ninner: {a: 2, b: two}\nafter: 3", Outer{1, {2, "two"}, 3});
  check_load<std::vector<WithSeq>>(
      "- name: x\n  values: [1, 2]\n- name: 'y'\n  values: []",
      std::vector<WithSeq>{{"x", {1, 2}}, {"y", {}}});
}

// The empty forms are the only flow style the serializer emits, so its own
// output does not round-trip until this works.
TEST_CASE("reflex::serde::yaml: the serializer's empty collections round-trip")
{
  check_load<WithSeq>(dump(WithSeq{"x", {}}), WithSeq{"x", {}});
  check_load<WithMap>(dump(WithMap{"x", {}}), WithMap{"x", {}});
  check_load<WithEmpty>(dump(WithEmpty{1, {}, 2}), WithEmpty{1, {}, 2});
  check_load<Empty>(dump(Empty{}), Empty{});
}

// YAML is a JSON superset, so anything the json backend writes has to parse
// here. Generated rather than hand-written, so it is real json output.
TEST_CASE("reflex::serde::yaml::deserializer: a JSON document parses as YAML")
{
  const Outer value{1, {2, "two"}, 3};

  std::string      out;
  json::serializer ser{out};
  ser.dump(value);
  CHECK(out == R"({"before":1,"inner":{"a":2,"b":"two"},"after":3})");

  check_load<Outer>(out, value);

  std::string      seq_out;
  json::serializer seq_ser{seq_out};
  seq_ser.dump(std::vector<Inner>{{1, "two"}, {2, "three"}});
  check_load<std::vector<Inner>>(seq_out, std::vector<Inner>{{1, "two"}, {2, "three"}});
}

// A map used to serialize without reading back, in every backend, for want of
// an object_visitor specialization. It reads back now.
TEST_CASE("reflex::serde::yaml::deserializer: a map round-trips")
{
  const auto m = std::map<std::string, int>{
      {"a", 1},
      {"b", 2}
  };
  CHECK(dump(m) == "a: 1\nb: 2");
  check_load<std::map<std::string, int>>("a: 1\nb: 2", m);

  const auto nested = std::map<std::string, Inner>{
      {"k", {1, "two"}}
  };
  CHECK(dump(nested) == "k:\n  a: 1\n  b: two");
  check_load<std::map<std::string, Inner>>("k:\n  a: 1\n  b: two", nested);

  check_load<WithMap>("name: x\nm:\n  a: 1", WithMap{"x", {{"a", 1}}});
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
