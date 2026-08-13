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

// Not an identifier scan over serde::deserializers(): that would only prove the
// name is a member of reflex::serde::de. with_deserializer additionally
// substitutes the template on the iterator type and constructs it from an
// iterator pair, and a backend can reflect correctly and still fail either.
TEST_CASE("toml is registered as a deserializer")
{
  const auto input = "x = 1"sv;
  bool       seen  = false;
  serde::with_deserializer("toml", input, [&seen](auto& de) {
    CHECK(std::remove_cvref_t<decltype(de)>::format_name == "TOML"sv);
    seen = true;
  });
  CHECK(seen);
}

template <typename T> static std::string dump(T const& value)
{
  std::string      out;
  toml::serializer ser{out};
  ser.dump(value);
  return out;
}

TEST_CASE("reflex::serde::toml: a string is a basic string by default")
{
  CHECK(dump("plain"s) == R"("plain")");
  CHECK(dump(""s) == R"("")");
  CHECK(dump("tab\there"s) == R"("tab\there")");
  CHECK(dump("two\nlines"s) == R"("two\nlines")");
}

TEST_CASE("reflex::serde::toml: a literal string is chosen when it saves an escape")
{
  // 'C:\path\to' beats "C:\\path\\to" for a reader, and needs no escape at all.
  CHECK(dump(R"(C:\path\to)"s) == R"('C:\path\to')");
  CHECK(dump(R"(a "quoted" word)"s) == R"('a "quoted" word')");
  CHECK(dump(R"(with"quote)"s) == R"('with"quote')");

  // A single quote cannot appear in a literal string and there is no doubling
  // rule to fall back on, so this goes basic even though it carries a backslash.
  CHECK(dump(R"(it's a \ mess)"s) == R"("it's a \\ mess")");

  // A control character rules the literal form out too.
  CHECK(dump("C:\\a\tb"s) == R"("C:\\a\tb")");

  // Nothing to save: no backslash and no double quote, so basic it is.
  CHECK(dump("it's plain"s) == R"("it's plain")");
}

TEST_CASE("reflex::serde::toml: control bytes use the 1.1 escape forms")
{
  // \e and \xHH are TOML 1.1 spellings. They are the only output this backend
  // produces that a TOML 1.0 parser rejects, and they are only reachable for a
  // control byte inside a string.
  CHECK_MESSAGE(dump("\x1B"s) == R"("\e")", "U+001B is \\e in 1.1, not \\u001b");
  CHECK_MESSAGE(dump("\x01"s) == R"("\x01")", "a bare control byte is \\xHH in 1.1, not \\u0001");
  CHECK(dump("\x7F"s) == R"("\x7f")");
  CHECK(dump("\b\t\n\f\r"s) == R"("\b\t\n\f\r")");

  // \0, \a and \v have no TOML spelling at all, unlike YAML.
  CHECK(dump("\0\a\v"s) == R"("\x00\x07\x0b")");
}

TEST_CASE("reflex::serde::toml: integers and floats are distinct spellings")
{
  CHECK(dump(1) == "1");
  CHECK(dump(-1) == "-1");
  CHECK(dump(std::int64_t{9007199254740993}) == "9007199254740993");
  CHECK(dump(1.5) == "1.5");

  // TOML's Float grammar wants an integer part plus a fractional or exponent
  // part, so a bare `1` is an Integer. to_chars is shortest-round-trip and
  // produces exactly that for the double 1.0, hence the appended ".0".
  CHECK(dump(1.0) == "1.0");
  CHECK(dump(-2.0) == "-2.0");
  CHECK(dump(0.0) == "0.0");
  CHECK(dump(1e30) == "1e+30");

  CHECK(dump(std::numeric_limits<double>::infinity()) == "inf");
  CHECK(dump(-std::numeric_limits<double>::infinity()) == "-inf");
  CHECK(dump(std::numeric_limits<double>::quiet_NaN()) == "nan");
  CHECK(dump(-std::numeric_limits<double>::quiet_NaN()) == "nan");
}

TEST_CASE("reflex::serde::toml: booleans and chars")
{
  CHECK(dump(true) == "true");
  CHECK(dump(false) == "false");
  CHECK(dump('x') == R"("x")");
  CHECK(dump('\'') == R"("'")");
}

// Namespace scope: a fixture declared inside a function body can fail to splice
// on GCC 16.
struct key_shapes
{
  int plain;
  [[= serde::rename{"with space"}]] int spaced;
  [[= serde::rename{"1st"}]] int        digit_first;
  [[= serde::rename{"a-b_c"}]] int      dashed;
};

TEST_CASE("reflex::serde::toml: a key is bare when it can be")
{
  // A bare key is [A-Za-z0-9_-]+ in 1.1 as in 1.0. An identifier always is one;
  // only a serde::rename can produce something else.
  static_assert(toml::detail::assign_key<^^key_shapes::plain>() == "plain = "sv);
  static_assert(toml::detail::assign_key<^^key_shapes::dashed>() == "a-b_c = "sv);
  static_assert(toml::detail::assign_key<^^key_shapes::digit_first>() == "1st = "sv);
  static_assert(toml::detail::assign_key<^^key_shapes::spaced>() == R"("with space" = )"sv);

  static_assert(toml::detail::key_name<^^key_shapes::spaced>() == R"("with space")"sv);

  CHECK(toml::detail::is_bare_key("abc"));
  CHECK(not toml::detail::is_bare_key(""));
  CHECK(not toml::detail::is_bare_key("a.b"));
}

TEST_CASE("reflex::serde::toml: a value with nowhere to go names the format")
{
  // TOML has no null. In a value position there is no key to omit, so this is
  // an error rather than a spelling question.
  const auto message = [](auto const& value) {
    try
    {
      dump(value);
    }
    catch(std::runtime_error const& e)
    {
      return std::string{e.what()};
    }
    return std::string{"no throw"};
  };

  CHECK(message(std::optional<int>{}).starts_with("TOML has no null"));
  CHECK(message(toml::null).starts_with("TOML has no null"));
  CHECK(dump(std::optional<int>{7}) == "7");
}

// Fixtures stay at namespace scope: a struct declared inside a function body
// can fail to splice on GCC 16.

struct flat_doc
{
  int         a;
  std::string b;
  bool        c;
};

TEST_CASE("reflex::serde::toml: a flat struct is one key per line")
{
  CHECK(dump(flat_doc{1, "x", true}) == "a = 1\nb = \"x\"\nc = true");
}

struct child_t
{
  int         x;
  std::string y;
};

// The order probe: `port` is declared AFTER the table member, so a one-pass
// writer emits it after "[tls]" and it silently becomes tls.port. The document
// stays valid TOML and means something else, which is why this case exists.
struct order_probe
{
  std::string host;
  child_t     tls;
  int         port;
};

TEST_CASE("reflex::serde::toml: every key of a table precedes its subtable headers")
{
  const auto out = dump(order_probe{"h", {1, "y"}, 8080});
  CHECK(out == "host = \"h\"\nport = 8080\n[tls]\nx = 1\ny = \"y\"");
  CHECK_MESSAGE(
      out.find("port = 8080") < out.find("[tls]"),
      "a key written after a [subtable] header belongs to that subtable");
}

struct two_tables_doc
{
  child_t first;
  child_t second;
};

TEST_CASE("reflex::serde::toml: one table block closes before the next opens")
{
  CHECK(
      dump(two_tables_doc{{1, "a"}, {2, "b"}})
      == "[first]\nx = 1\ny = \"a\"\n[second]\nx = 2\ny = \"b\"");
}

struct lvl_c_t
{
  int v;
};
struct lvl_b_t
{
  int     v;
  lvl_c_t c;
};
struct lvl_a_t
{
  int     v;
  lvl_b_t b;
};
struct lvl_doc
{
  lvl_a_t a;
};

TEST_CASE("reflex::serde::toml: a header path is every enclosing key")
{
  CHECK(dump(lvl_doc{{1, {2, {3}}}}) == "[a]\nv = 1\n[a.b]\nv = 2\n[a.b.c]\nv = 3");
}

struct empty_child_t
{};
struct empty_table_doc
{
  int           a;
  empty_child_t child;
};

TEST_CASE("reflex::serde::toml: an empty table still gets its header")
{
  // [child] with nothing under it is a distinct TOML value; dropping the header
  // would lose it.
  CHECK(dump(empty_table_doc{1, {}}) == "a = 1\n[child]");
}

struct nums_doc
{
  std::vector<int> items;
};

TEST_CASE("reflex::serde::toml: a sequence of leaves is an inline array")
{
  CHECK(dump(nums_doc{{1, 2, 3}}) == "items = [1, 2, 3]");
  CHECK(dump(nums_doc{}) == "items = []");
}

struct items_doc
{
  std::vector<child_t> items;
};

TEST_CASE("reflex::serde::toml: a sequence of tables is an array of tables")
{
  CHECK(
      dump(items_doc{{{1, "a"}, {2, "b"}}})
      == "[[items]]\nx = 1\ny = \"a\"\n[[items]]\nx = 2\ny = \"b\"");

  // Nothing at all, not a header: [[items]] with no body is one element, not
  // zero.
  CHECK(dump(items_doc{}).empty());
}

struct leaf_item
{
  int n;
};
struct group_item
{
  std::string            name;
  std::vector<leaf_item> leaves;
};
struct groups_doc
{
  std::vector<group_item> groups;
};

TEST_CASE("reflex::serde::toml: an array of tables nests through the path")
{
  CHECK(
      dump(groups_doc{{{"g1", {{1}, {2}}}, {"g2", {}}}})
      == "[[groups]]\nname = \"g1\"\n[[groups.leaves]]\nn = 1\n[[groups.leaves]]\nn = 2\n"
         "[[groups]]\nname = \"g2\"");
}

struct grid_doc
{
  std::vector<std::vector<child_t>> rows;
};

TEST_CASE("reflex::serde::toml: inside an array a table has to go inline")
{
  // The outer sequence's value_type is a sequence, not a table, so it stays an
  // inline array - and an inline array has no line for a header to sit on.
  CHECK(
      dump(grid_doc{{{{1, "a"}}, {{2, "b"}}}})
      == R"(rows = [[{ x = 1, y = "a" }], [{ x = 2, y = "b" }]])");
}

struct map_of_tables_doc
{
  std::map<std::string, child_t> m;
};

TEST_CASE("reflex::serde::toml: a map of tables gets a header per entry")
{
  // The map member gets its own [m] the way any other table member does; the
  // entries hang off it. A quoted entry key stays quoted inside the path.
  CHECK(
      dump(map_of_tables_doc{{{"key1", {1, "a"}}, {"key2", {2, "b"}}}})
      == "[m]\n[m.key1]\nx = 1\ny = \"a\"\n[m.key2]\nx = 2\ny = \"b\"");
  CHECK(
      dump(map_of_tables_doc{{{"has space", {1, "a"}}}})
      == "[m]\n[m.\"has space\"]\nx = 1\ny = \"a\"");
}

struct map_of_leaves_doc
{
  std::map<std::string, int> m;
};

TEST_CASE("reflex::serde::toml: a map of leaves is a table of keys")
{
  CHECK(dump(map_of_leaves_doc{{{"a", 1}, {"b", 2}}}) == "[m]\na = 1\nb = 2");
  CHECK(dump(map_of_leaves_doc{}) == "[m]");
}

struct opt_doc
{
  int                a;
  std::optional<int> b;
  int                c;
};
struct opt_table_doc
{
  int                    a;
  std::optional<child_t> child;
};

TEST_CASE("reflex::serde::toml: an empty optional takes its key with it")
{
  CHECK(dump(opt_doc{1, std::nullopt, 3}) == "a = 1\nc = 3");
  CHECK(dump(opt_doc{1, 2, 3}) == "a = 1\nb = 2\nc = 3");
  CHECK(dump(opt_table_doc{1, std::nullopt}) == "a = 1");
  CHECK(dump(opt_table_doc{1, child_t{2, "z"}}) == "a = 1\n[child]\nx = 2\ny = \"z\"");

  // An inline table has a key to omit too.
  CHECK(dump(std::vector<opt_doc>{{1, std::nullopt, 3}}) == "[{ a = 1, c = 3 }]");

  // An array element does not, and that is the format's limit rather than this
  // backend's: `x = [1, , 3]` does not exist.
  std::string message;
  try
  {
    dump(std::vector<std::optional<int>>{1, std::nullopt});
  }
  catch(std::runtime_error const& e)
  {
    message = e.what();
  }
  CHECK(message.starts_with("TOML has no null"));
}

struct renamed_doc
{
  [[= serde::rename{"with space"}]] int     v;
  [[= serde::rename{"my table"}]] child_t   t;
};

TEST_CASE("reflex::serde::toml: a renamed key is quoted on both sides")
{
  CHECK(dump(renamed_doc{1, {2, "a"}}) == "\"with space\" = 1\n[\"my table\"]\nx = 2\ny = \"a\"");
}

TEST_CASE("reflex::serde::toml: a pair is a one-entry table")
{
  CHECK(dump(std::pair{"k"s, 42}) == "k = 42");
  CHECK(dump(std::vector{std::pair{"k"s, 42}}) == "[{ k = 42 }]");
}
