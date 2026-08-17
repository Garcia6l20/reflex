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

// Every case runs twice: over a contiguous buffer, and through a stream cursor
// with bulk_scan off. The parser is one implementation, so the two must agree
// exactly.
//
// Each input below is a bare value. `x = <value>` around any of them is what a
// TOML parser is fed, and that is how they were cross-checked against Python's
// tomllib.

namespace
{
  template <typename Fn> void both_cursors(std::string_view text, Fn&& fn)
  {
    {
      toml::deserializer de{text};
      fn(de);
    }
    {
      std::istringstream in{std::string{text}};
      toml::deserializer de{in};
      fn(de);
    }
  }

  template <typename T> void check_load(std::string_view text, T const& expected)
  {
    CHECK(toml::deserializer{text}.template load<T>() == expected);
    std::istringstream in{std::string{text}};
    CHECK(toml::deserializer{in}.template load<T>() == expected);
  }

  template <typename T> void check_load_throws(std::string_view text)
  {
    CHECK_THROWS(toml::deserializer{text}.template load<T>());
    std::istringstream in{std::string{text}};
    CHECK_THROWS(toml::deserializer{in}.template load<T>());
  }
} // namespace

// "mlb-quotes = 1*2quotation-mark" then the three-quote delimiter, so a run of
// four ends the string with one quote of content and a run of five with two.
// The quote counts are spelled out because a literal run of them is unreadable.
TEST_CASE("reflex::serde::toml::deserializer: the closing quote run")
{
  check_load<std::string>(R"("""""")", "");           // 3 + 3
  check_load<std::string>(R"(""""""")", "\"");        // 3 + 4 -> one quote
  check_load<std::string>(R"("""""""")", "\"\"");     // 3 + 5 -> two quotes
  check_load_throws<std::string>(R"("""a""""""")");   // 3 + a + 7
  check_load_throws<std::string>(R"("""""""""")");    // 3 + 7

  // One or two quotes anywhere else are ordinary content.
  check_load<std::string>(R"("""a"b""")", "a\"b");
  check_load<std::string>(R"("""a""b""")", "a\"\"b");

  // A literal multi-line string counts its apostrophes the same way.
  check_load<std::string>(R"('''''')", "");
  check_load<std::string>(R"(''''''')", "'");
  check_load<std::string>(R"('''''''')", "''");
}

// A trailing backslash deletes the break and every space, tab and blank line
// after it. This is not yaml's folding, which turns a break into a space.
TEST_CASE("reflex::serde::toml::deserializer: a trailing backslash deletes the break")
{
  check_load<std::string>("\"\"\"a\\\n   b\"\"\"", "ab");
  check_load<std::string>("\"\"\"a\\\n\n\n   b\"\"\"", "ab");
  // "mlb-escaped-nl = escape ws newline ...": whitespace may sit between the
  // backslash and the break.
  check_load<std::string>("\"\"\"a\\  \t\n   b\"\"\"", "ab");

  // Without the backslash the break and the indent are content.
  check_load<std::string>("\"\"\"a\n   b\"\"\"", "a\n   b");

  // A single-line basic string has no continuation rule: a break after the
  // backslash is an error, not a fold.
  check_load_throws<std::string>("\"a\\\nb\"");
}

TEST_CASE("reflex::serde::toml::deserializer: the four string forms")
{
  check_load<std::string>(R"("basic")", "basic");
  check_load<std::string>(R"("")", "");
  check_load<std::string>(R"('literal')", "literal");
  check_load<std::string>(R"('')", "");

  // A literal string has no escape mechanism at all.
  check_load<std::string>(R"('C:\path\to')", R"(C:\path\to)");
  check_load<std::string>(R"('\n')", R"(\n)");

  // "A newline immediately following the opening delimiter will be trimmed."
  // Only the first one.
  check_load<std::string>("\"\"\"\n  a\n  b\"\"\"", "  a\n  b");
  check_load<std::string>("\"\"\"\n\na\"\"\"", "\na");
  check_load<std::string>("'''\nline\n'''", "line\n");

  // Neither single-line form may carry a break.
  check_load_throws<std::string>("\"a\nb\"");
  check_load_throws<std::string>("'a\nb'");
  check_load_throws<std::string>(R"("unterminated)");

  // "basic-unescaped = wschar / %x21 / %x23-5B / %x5D-7E / non-ascii": tab is
  // in, every other control byte and DEL are out.
  check_load<std::string>("\"a\tb\"", "a\tb");
  check_load_throws<std::string>("\"a\x01 b\"");
  check_load_throws<std::string>("'a\x7F b'");
}

TEST_CASE("reflex::serde::toml::deserializer: the 1.1 escape set")
{
  check_load<std::string>(R"("\b\t\n\f\r\"\\")", "\b\t\n\f\r\"\\");
  check_load<std::string>(R"("\e")", "\x1B");
  check_load<std::string>(R"("\x41")", "A");
  check_load<std::string>(R"("\u0041")", "A");
  check_load<std::string>(R"("\U00000041")", "A");

  // \0, \a and \v have no TOML spelling, and neither does JSON's \/.
  check_load_throws<std::string>(R"("\0")");
  check_load_throws<std::string>(R"("\a")");
  check_load_throws<std::string>(R"("\v")");
  check_load_throws<std::string>(R"("\/")");
  check_load_throws<std::string>(R"("\q")");
  check_load_throws<std::string>(R"("\xZZ")");
  check_load_throws<std::string>(R"("\u00")");

  // U+0080 is two bytes of UTF-8 and this backend decodes only the subset
  // below it - the same limit json and yaml carry, and exactly what the
  // serializer emits. A literal multi-byte sequence passes through untouched.
  check_load_throws<std::string>(R"("\x80")");
  check_load_throws<std::string>(R"("\u00e9")");
  check_load<std::string>("\"caf\xC3\xA9\"", "caf\xC3\xA9");
}

TEST_CASE("reflex::serde::toml::deserializer: integers in four bases")
{
  check_load<int>("0", 0);
  check_load<int>("+1", 1);
  check_load<int>("-1", -1);
  check_load<int>("1_000", 1000);
  check_load<std::int64_t>("9007199254740993", 9007199254740993LL);
  check_load<std::int64_t>("0xdead_beef", 0xdeadbeefLL);
  check_load<int>("0o755", 493);
  check_load<int>("0b1010", 10);
  check_load<int>("0x0", 0);

  // An underscore is only legal between two digits.
  check_load_throws<int>("1__0");
  check_load_throws<int>("_1");
  check_load_throws<int>("1_");

  // "unsigned-dec-int = DIGIT / digit1-9 1*( DIGIT / underscore DIGIT )": a
  // lone zero and nothing else that starts with one.
  check_load_throws<int>("0123");
  check_load_throws<int>("00");

  // A prefixed form takes no sign.
  check_load_throws<int>("-0x1");
  check_load_throws<int>("+0b1");

  check_load_throws<int>("abc");
  check_load_throws<int>(R"("42")");
}

TEST_CASE("reflex::serde::toml::deserializer: floats")
{
  check_load<double>("1.5", 1.5);
  check_load<double>("-1.5", -1.5);
  check_load<double>("1e10", 1e10);
  check_load<double>("6.02e23", 6.02e23);
  check_load<double>("1e0_1", 10.0);
  check_load<double>("-inf", -std::numeric_limits<double>::infinity());
  check_load<double>("+inf", std::numeric_limits<double>::infinity());
  check_load<double>("inf", std::numeric_limits<double>::infinity());

  // A TOML Integer still reads into a floating-point destination; what the
  // format calls the value and what the schema asks for are two questions.
  check_load<double>("1", 1.0);

  // "float = float-int-part ( exp / frac [ exp ] )" with
  // "frac = decimal-point zero-prefixable-int": both halves are required.
  check_load_throws<double>("1.");
  check_load_throws<double>(".5");
  check_load_throws<double>("01.5");
  check_load_throws<double>("1e");
  check_load_throws<double>("INF");
  check_load_throws<double>("NaN");

  CHECK(std::isnan(toml::deserializer{"nan"sv}.load<double>()));
  CHECK(std::isnan(toml::deserializer{"-nan"sv}.load<double>()));
}

TEST_CASE("reflex::serde::toml::deserializer: booleans are lowercase")
{
  check_load<bool>("true", true);
  check_load<bool>("false", false);
  check_load_throws<bool>("True");
  check_load_throws<bool>("TRUE");
  check_load_throws<bool>("yes");
}

// Recognised by shape and read verbatim. Seconds are optional as of 1.1, so
// "07:32" is a local time rather than a truncated one.
TEST_CASE("reflex::serde::toml::deserializer: a date-time is read as a string")
{
  check_load<std::string>("1979-05-27T07:32:00Z", "1979-05-27T07:32:00Z");
  check_load<std::string>("1979-05-27 07:32:00-07:00", "1979-05-27 07:32:00-07:00");
  check_load<std::string>("1979-05-27T07:32:00", "1979-05-27T07:32:00");
  check_load<std::string>("1979-05-27t07:32:00.999", "1979-05-27t07:32:00.999");
  check_load<std::string>("1979-05-27", "1979-05-27");
  check_load<std::string>("07:32:00", "07:32:00");
  check_load<std::string>("07:32", "07:32");

  // The space separator only continues the token when a time really follows.
  both_cursors("1979-05-27 # today", [](auto& de) {
    CHECK(de.template load<std::string>() == "1979-05-27");
    CHECK_NOTHROW(de.finish_line());
  });

  // A real limitation rather than a parse error, and the message has to say so.
  std::string message;
  try
  {
    (void)toml::deserializer{"1979-05-27"sv}.load<int>();
  }
  catch(std::runtime_error const& e)
  {
    message = e.what();
  }
  CHECK(message.starts_with("TOML: a date-time is read as a string"));

  check_load_throws<std::string>("12:ab");
  check_load_throws<std::string>("42");
}

TEST_CASE("reflex::serde::toml::deserializer: whitespace, comments and line ends")
{
  both_cursors("  \t x", [](auto& de) {
    de.skip_ws();
    CHECK(de.column() == 4);
    CHECK(de.peek() == 'x');
  });

  both_cursors("\n\n  # a comment\t\nvalue", [](auto& de) {
    CHECK(de.next_content_line());
    CHECK(de.peek() == 'v');
  });
  both_cursors("# only a comment", [](auto& de) { CHECK(not de.next_content_line()); });
  both_cursors("", [](auto& de) { CHECK(not de.next_content_line()); });

  // Idempotent once parked on content: read_document()'s loop asks about a line
  // a value reader has already stopped on.
  both_cursors("a\nb", [](auto& de) {
    CHECK(de.next_content_line());
    CHECK(de.next_content_line());
    CHECK(de.peek() == 'a');
  });

  // 1.1 bars a control character other than tab inside a comment.
  both_cursors("1 # tab\there", [](auto& de) {
    CHECK(de.template load<int>() == 1);
    CHECK_NOTHROW(de.finish_line());
  });
  both_cursors("1 # bell\a", [](auto& de) {
    CHECK(de.template load<int>() == 1);
    CHECK_THROWS(de.finish_line());
  });

  // Junk after a value is caught where it happens rather than left to
  // desynchronise the next line.
  both_cursors("1 junk", [](auto& de) {
    CHECK(de.template load<int>() == 1);
    CHECK_THROWS(de.finish_line());
  });
}

TEST_CASE("reflex::serde::toml::deserializer: TOML has no null")
{
  check_load_throws<toml::null_t>("");
  // An optional is an absent key, which only a table can see, so a value
  // position simply reads the payload.
  check_load<std::optional<int>>("7", std::optional<int>{7});
}

struct ab_doc
{
  int         a;
  std::string b;
  bool        operator==(ab_doc const&) const = default;
};

struct inner_doc
{
  int  b;
  bool operator==(inner_doc const&) const = default;
};

struct dotted_doc
{
  inner_doc a;
  bool      operator==(dotted_doc const&) const = default;
};

TEST_CASE("reflex::serde::toml::deserializer: arrays")
{
  check_load<std::vector<int>>("[1, 2, 3]", {1, 2, 3});
  check_load<std::vector<int>>("[]", {});
  check_load<std::vector<int>>("[ ]", {});
  check_load<std::vector<std::string>>(R"(["a", 'b'])", {"a", "b"});

  // A trailing comma has been legal in an array since 1.0.
  check_load<std::vector<int>>("[1, 2, 3,]", {1, 2, 3});
  check_load<std::vector<int>>("[1,]", {1});

  check_load<std::vector<std::vector<int>>>("[[1,2],[3,4]]", {{1, 2}, {3, 4}});

  // An array spans as many lines as it likes, with comments anywhere between
  // two tokens.
  check_load<std::vector<int>>(
      "[\n"
      "  1, # one\n"
      "  2,\n"
      "  # a whole comment line\n"
      "  3,\n"
      "]",
      {1, 2, 3});

  check_load<std::array<int, 3>>("[1, 2, 3]", {1, 2, 3});
  check_load_throws<std::array<int, 2>>("[1, 2, 3]");

  check_load_throws<std::vector<int>>("[1, 2");
  check_load_throws<std::vector<int>>("[1 2]");
  check_load_throws<std::vector<int>>("1, 2]");
}

TEST_CASE("reflex::serde::toml::deserializer: a mixed array fails on the element")
{
  // Mixed elements are valid TOML. Reading one into a std::vector<int> is the
  // destination's problem, and the message has to name the format.
  std::string message;
  try
  {
    (void)toml::deserializer{R"([1, "two"])"sv}.load<std::vector<int>>();
  }
  catch(std::runtime_error const& e)
  {
    message = e.what();
  }
  CHECK(message.starts_with("TOML: "));
}

TEST_CASE("reflex::serde::toml::deserializer: inline tables")
{
  check_load<ab_doc>(R"({ a = 1, b = "x" })", {1, "x"});
  check_load<ab_doc>(R"({a=1,b='x'})", {1, "x"});
  check_load<std::map<std::string, int>>("{ a = 1, b = 2 }", {{"a", 1}, {"b", 2}});
  check_load<std::map<std::string, int>>("{}", {});
  check_load<std::map<std::string, int>>("{ }", {});

  // 1.1 allows both a line break and a trailing comma inside an inline table.
  // 1.0 allowed neither, which is why this reader and the writer differ here.
  check_load<ab_doc>(
      "{\n"
      "  a = 1, # first\n"
      "  b = \"x\",\n"
      "}",
      {1, "x"});

  check_load_throws<ab_doc>(R"({ a = 1, b = "x" )");
  check_load_throws<ab_doc>(R"({ a = 1 b = "x" })");
  check_load_throws<ab_doc>(R"({ a })");
  check_load_throws<ab_doc>(R"({ = 1 })");
}

TEST_CASE("reflex::serde::toml::deserializer: a dotted key reaches a nested member")
{
  check_load<dotted_doc>("{ a.b = 1 }", {{1}});
  // "dot-sep = ws %x2E ws": whitespace may sit on either side of the dot.
  check_load<dotted_doc>("{ a . b = 1 }", {{1}});
  // A quoted segment is a segment, not a name carrying a dot.
  check_load<dotted_doc>(R"({ "a".'b' = 1 })", {{1}});
  // The nested table written out in full means the same thing.
  check_load<dotted_doc>("{ a = { b = 1 } }", {{1}});
  // A multi-line string is not a key.
  check_load_throws<dotted_doc>(R"({ """a""".b = 1 })");
}

// Every document below was fed to Python's tomllib and compared against the
// shape asserted here. The failure mode is not a parse error, it is a key
// landing in the wrong table, which produces a valid document that no literal
// comparison catches.

struct tls_conf
{
  bool        enabled;
  std::string cert;
  bool        operator==(tls_conf const&) const = default;
};

struct server_conf
{
  std::string host;
  int         port;
  tls_conf    tls;
  bool        operator==(server_conf const&) const = default;
};

struct doc_conf
{
  std::string title;
  server_conf server;
  bool        operator==(doc_conf const&) const = default;
};

struct deep_c
{
  int  n;
  bool operator==(deep_c const&) const = default;
};

struct deep_b
{
  deep_c c;
  bool   operator==(deep_b const&) const = default;
};

struct deep_a
{
  deep_b c;
  deep_b b;
  bool   operator==(deep_a const&) const = default;
};

struct deep_doc
{
  deep_a  a;
  deep_c  d;
  int     top;
  bool    operator==(deep_doc const&) const = default;
};

struct map_doc
{
  std::map<std::string, inner_doc> m;
  bool operator==(map_doc const&) const = default;
};

TEST_CASE("reflex::serde::toml::deserializer: a flat document is one key per line")
{
  check_load<ab_doc>("a = 1\nb = \"x\"", {1, "x"});
  check_load<ab_doc>("# a comment\n\na = 1   # trailing\n\nb = 'x'\n", {1, "x"});
  // A document is the one place a table needs no braces at all.
  check_load<std::map<std::string, int>>("a = 1\nb = 2", {{"a", 1}, {"b", 2}});
  // An empty document is an empty table, not an error.
  check_load<std::map<std::string, int>>("", {});
  check_load<std::map<std::string, int>>("# nothing but a comment\n", {});

  // "keyval-sep = ws %x3D ws": the value is on the line the key is on.
  check_load_throws<ab_doc>("a =\n1");
  check_load_throws<ab_doc>("a 1");
  check_load_throws<ab_doc>("= 1");
  // A value may still run on to the next line once it has opened a bracket.
  check_load<std::vector<int>>("[1,\n2]", {1, 2});
}

TEST_CASE("reflex::serde::toml::deserializer: a header re-targets the lines after it")
{
  const auto text =
      "title = \"cfg\"\n"
      "[server]\n"
      "host = \"localhost\"\n"
      "port = 8080\n"
      "[server.tls]\n"
      "enabled = true\n"
      "cert = \"/etc/cert\"\n";
  check_load<doc_conf>(text, {"cfg", {"localhost", 8080, {true, "/etc/cert"}}});

  // The header is indentation-blind and may carry whitespace inside the
  // brackets: "[ a . b ]" is the same header as "[a.b]".
  check_load<doc_conf>(
      "title = \"cfg\"\n"
      "     [ server . tls ]   # here\n"
      "enabled = true\n",
      {"cfg", {"", 0, {true, ""}}});
}

TEST_CASE("reflex::serde::toml::deserializer: a header jumps back up as easily as down")
{
  // Three levels down and then straight back to one, which is what no
  // recursive-descent reader can express.
  const auto text =
      "top = 1\n"
      "[a.c.c]\n"
      "n = 2\n"
      "[a.b.c]\n"
      "n = 3\n"
      "[d]\n"
      "n = 4\n";
  check_load<deep_doc>(text, {{{{2}}, {{3}}}, {4}, 1});
}

TEST_CASE("reflex::serde::toml::deserializer: a dotted key needs no header")
{
  check_load<dotted_doc>("a.b = 1", {{1}});
  check_load<dotted_doc>("a . b = 1", {{1}});
  check_load<deep_doc>("a.b.c.n = 3\nd.n = 4\ntop = 1", {{{}, {{3}}}, {4}, 1});

  // A quoted segment is one segment, so "a.b.c" here is three keys and
  // 'a."b.c".d' is three keys as well - not four.
  check_load<dotted_doc>(R"("a".'b' = 1)", {{1}});

  // A dotted key under a header appends to the header's path.
  check_load<deep_doc>("[a]\nb.c.n = 3\n", {{{}, {{3}}}, {}, 0});
}

TEST_CASE("reflex::serde::toml::deserializer: a map is filled by headers")
{
  check_load<map_doc>("[m.one]\nb = 1\n[m.two]\nb = 2\n", {{{"one", {1}}, {"two", {2}}}});
  // The same entries reached without a header for the map itself.
  check_load<map_doc>("m.one.b = 1\nm.two.b = 2\n", {{{"one", {1}}, {"two", {2}}}});
}

namespace
{
  template <typename T> std::string load_message(std::string_view text)
  {
    try
    {
      (void)toml::deserializer{text}.template load<T>();
    }
    catch(std::exception const& e)
    {
      return e.what();
    }
    return {};
  }
} // namespace

TEST_CASE("reflex::serde::toml::deserializer: an unknown key names the format and the path")
{
  // object_visit says "Key not found in object", which names neither the
  // format nor where in the document it happened. Wrapped at the call site
  // rather than reworded there: four other things read that message.
  const std::string message = load_message<doc_conf>("[server.tls]\nnope = true\n");
  CHECK(message.starts_with("TOML: "));
  CHECK(message.find("server.tls.nope") != std::string::npos);

  // A message this backend wrote already carries both and is left alone.
  CHECK(load_message<doc_conf>("[server]\nport = \"x\"\n").starts_with("TOML: "));
}

// An inline table's dotted key takes the same indexed walk a document-level
// assignment takes, so a segment landing on something that is not a table is
// reported rather than writing the value into the enclosing object.
TEST_CASE("reflex::serde::toml::deserializer: an inline dotted key cannot reach through a non-table")
{
  const std::string message = load_message<ab_doc>("{ a.b = 1 }");
  CHECK(message.starts_with("TOML: "));
  CHECK(message.find("a.b") != std::string::npos);
  CHECK(message.find("is not a table") != std::string::npos);

  // The value the document wrote happens to fit the parent, so a walk handing
  // back the parent would assign it silently rather than fail.
  check_load_throws<ab_doc>(R"({ a.b = { a = 5, b = "z" } })");

  // One level in, and under a header rather than at the root.
  check_load_throws<doc_conf>("[server]\ntls = { enabled.x = true }\n");

  // The document-level form of the same key already said this, and still does.
  CHECK(load_message<ab_doc>("a.b = 1\n").find("is not a table") != std::string::npos);
}

TEST_CASE("reflex::serde::toml::deserializer: a path may only be defined once")
{
  // A duplicate key, directly and through a header.
  check_load_throws<ab_doc>("a = 1\na = 2");
  check_load_throws<map_doc>("[m.one]\nb = 1\nb = 2\n");
  check_load_throws<deep_doc>("a.b.c.n = 1\na.b.c.n = 2\n");

  // A duplicate header, and a header on a path an assignment already took.
  check_load_throws<map_doc>("[m.one]\n[m.one]\n");
  check_load_throws<deep_doc>("[a.b]\n[a.b]\n");
  check_load_throws<map_doc>("[m]\none = { b = 1 }\n[m.one]\nb = 2\n");
  check_load_throws<deep_doc>("[a]\nb = { c = { n = 1 } }\n[a.b.c]\n");

  // A dotted key cannot redefine a table a header defined, and a header cannot
  // redefine a table a dotted key created.
  check_load_throws<deep_doc>("[a.b]\nc.n = 1\n[a]\nb.c.n = 2\n");
  check_load_throws<deep_doc>("[a]\nb.c.n = 1\n[a.b]\n");

  check_load_throws<deep_doc>("[a]\n[a.b]\n[a]\n");

  // Legal, and the openings the spec leaves: a super-table written after the
  // sub-table that implied it, a second sibling under a dotted key, and a
  // sub-table added by a header under a table a dotted key created.
  check_load<deep_doc>("[a.b.c]\nn = 3\n[a]\n", {{{}, {{3}}}, {}, 0});
  check_load<deep_doc>("[a.b]\nc.n = 1\n[d]\nn = 4\n[a]\n", {{{}, {{1}}}, {4}, 0});
  check_load<deep_a>("c.c.n = 1\nb.c.n = 2\n", {{{1}}, {{2}}});
  check_load<deep_doc>("a.c.c.n = 1\n[a.b]\nc.n = 2\n", {{{{1}}, {{2}}}, {}, 0});
}

// Arrays of tables: the only paths that carry an index, and the reason this
// backend has a walk of its own on top of object_visitor.

struct leaf_row
{
  int  n;
  bool operator==(leaf_row const&) const = default;
};

struct table_row
{
  std::string           name;
  leaf_row              sub;
  std::vector<leaf_row> leaves;
  bool                  operator==(table_row const&) const = default;
};

struct rows_doc
{
  std::vector<table_row> items;
  std::vector<table_row> more;
  bool                   operator==(rows_doc const&) const = default;
};

struct fixed_rows_doc
{
  std::array<leaf_row, 2> items;
  bool                    operator==(fixed_rows_doc const&) const = default;
};

struct scalar_rows_doc
{
  int  items;
  bool operator==(scalar_rows_doc const&) const = default;
};

TEST_CASE("reflex::serde::toml::deserializer: [[header]] appends an element")
{
  check_load<rows_doc>(
      "[[items]]\n"
      "name = \"a\"\n"
      "[[items]]\n"
      "name = \"b\"\n",
      {{{"a", {}, {}}, {"b", {}, {}}}, {}});

  // The same key in two elements is not a duplicate: an element is a table of
  // its own. In one element it is.
  check_load_throws<rows_doc>("[[items]]\nname = \"a\"\nname = \"b\"\n");
}

TEST_CASE("reflex::serde::toml::deserializer: a header after [[header]] targets the last element")
{
  check_load<rows_doc>(
      "[[items]]\n"
      "name = \"a\"\n"
      "[items.sub]\n"
      "n = 7\n"
      "[[items]]\n"
      "name = \"b\"\n"
      "[items.sub]\n"
      "n = 8\n",
      {{{"a", {7}, {}}, {"b", {8}, {}}}, {}});

  // A dotted key inside an element reaches the same place.
  check_load<rows_doc>("[[items]]\nsub.n = 7\n", {{{"", {7}, {}}}, {}});
}

TEST_CASE("reflex::serde::toml::deserializer: interleaved arrays of tables keep their counts")
{
  // "[[a]] [[b]] [[a]]" is legal and the second [[a]] is element 1: a count is
  // never reset, only ever appended to.
  check_load<rows_doc>(
      "[[items]]\n"
      "name = \"a\"\n"
      "[[more]]\n"
      "name = \"x\"\n"
      "[[items]]\n"
      "name = \"b\"\n",
      {{{"a", {}, {}}, {"b", {}, {}}}, {{"x", {}, {}}}});
}

TEST_CASE("reflex::serde::toml::deserializer: an array of tables nests inside an element")
{
  // The inner count belongs to the element, not to the name: the leaves of
  // items[1] start again at 0.
  check_load<rows_doc>(
      "[[items]]\n"
      "name = \"a\"\n"
      "[[items.leaves]]\n"
      "n = 1\n"
      "[[items.leaves]]\n"
      "n = 2\n"
      "[[items]]\n"
      "name = \"b\"\n"
      "[[items.leaves]]\n"
      "n = 3\n",
      {{{"a", {}, {{1}, {2}}}, {"b", {}, {{3}}}}, {}});
}

TEST_CASE("reflex::serde::toml::deserializer: an array of tables fits its destination or throws")
{
  // A fixed-size destination cannot grow, so it is bounds-checked instead -
  // the same split make_pusher encodes for an inline array.
  check_load<fixed_rows_doc>("[[items]]\nn = 1\n[[items]]\nn = 2\n", {{{{1}, {2}}}});
  check_load_throws<fixed_rows_doc>("[[items]]\nn = 1\n[[items]]\nn = 2\n[[items]]\nn = 3\n");

  // A destination that is not a sequence at all says so.
  const std::string message = load_message<scalar_rows_doc>("[[items]]\nn = 1\n");
  CHECK(message.starts_with("TOML: "));
  CHECK(message.find("sequence") != std::string::npos);
}

TEST_CASE("reflex::serde::toml::deserializer: a path is a table or an array of tables, not both")
{
  check_load_throws<rows_doc>("[[items]]\n[items]\n");
  check_load_throws<map_doc>("[m]\n[[m]]\n");
  check_load_throws<rows_doc>("items = []\n[[items]]\n");
}

// What bounds a walk is the segment count, and that is bounded where the path
// is read: serde::max_key_depth is 32.
TEST_CASE("reflex::serde::toml::deserializer: a key past 32 segments is refused")
{
  std::string text;
  for(int i = 0; i < 32; ++i)
  {
    text += "a.";
  }
  text += "a = 1";
  CHECK(load_message<map_doc>(text).starts_with("TOML: "));
}

// The schema-free path: no destination type steers the read, so the shape of
// the text decides.

TEST_CASE("reflex::serde::toml: a document reads into a toml::value")
{
  const auto text =
      "i = 42\n"
      "big = 9007199254740993\n"       // 2^53 + 1, unrepresentable as a double
      "f = 1.5\n"
      "one = 1.0\n"
      "b = true\n"
      "s = \"text\"\n"
      "when = 1979-05-27T07:32:00Z\n"
      "list = [1, 2, \"three\"]\n"
      "inline = { k = 1 }\n"
      "[table]\n"
      "n = 7\n"
      "[[items]]\n"
      "n = 1\n"
      "[[items]]\n"
      "n = 2\n"sv;

  auto v = toml::deserializer{text}.load<toml::value>();

  // TOML's Integer and Float are two types, not two spellings of one, so `1`
  // and `1.0` land in different alternatives and the integer stays exact.
  CHECK(v["i"].is<toml::integer>());
  CHECK(v["i"] == 42);
  CHECK(v["big"].as<toml::integer>() == 9007199254740993LL);
  CHECK(v["f"].is<toml::number>());
  CHECK(v["one"].is<toml::number>());
  CHECK(v["b"].is<toml::boolean>());
  CHECK(v["s"].as<toml::string>() == "text");
  // Date-times are carried verbatim as strings; nothing here maps them onto a
  // calendar type.
  CHECK(v["when"].as<toml::string>() == "1979-05-27T07:32:00Z");
  CHECK(v["list"].is_array());
  CHECK(v["list"].size() == 3);
  CHECK(v["inline"]["k"] == 1);
  CHECK(v["table"]["n"] == 7);
  CHECK(v["items"].is_array());
  CHECK(v["items"].size() == 2);
  CHECK(v["items"][1]["n"] == 2);

  // Written back and read again. The second load is what catches a lossy
  // write: a value that comes out in the wrong form reads back as the wrong
  // alternative even though the document parses.
  const std::string out = dump(v);
  auto back = toml::deserializer{std::string_view{out}}.load<toml::value>();
  CHECK(back == v);
  CHECK(back["big"].as<toml::integer>() == 9007199254740993LL);
  CHECK(back["one"].is<toml::number>());
  CHECK(back["i"].is<toml::integer>());
}

TEST_CASE("reflex::serde::toml: a header builds objects in a toml::value")
{
  both_cursors("[a.b.c]\nn = 1\n[a.d]\nm = 2\n", [](auto& de) {
    auto v = de.template load<toml::value>();
    CHECK(v["a"].is_object());
    CHECK(v["a"]["b"].is_object());
    CHECK(v["a"]["b"]["c"]["n"] == 1);
    CHECK(v["a"]["d"]["m"] == 2);
  });

  // A header with nothing under it is still a table. A typed destination has
  // the member either way, but a toml::value only has what the document put
  // there.
  auto empty = toml::deserializer{"[a]"sv}.load<toml::value>();
  CHECK(empty["a"].is_object());
  CHECK(empty["a"].size() == 0);
}

// The entry the visitor creates for a missing key holds null, and null has no
// members. The indexed walk makes a null into a table on the way down, the same
// way a [header] path does, which is what lets a dotted key inside an inline
// table reach a schema-free destination.
TEST_CASE("reflex::serde::toml: an inline dotted key builds objects in a toml::value")
{
  auto v = toml::deserializer{"t = { a.b = 1, a.c = 2, d = 3 }\n"sv}.load<toml::value>();
  CHECK(v["t"]["a"].is_object());
  CHECK(v["t"]["a"]["b"] == 1);
  CHECK(v["t"]["a"]["c"] == 2);
  CHECK(v["t"]["d"] == 3);
}

TEST_CASE("reflex::serde::toml: an array of tables builds an arr in a toml::value")
{
  auto v = toml::deserializer{"[[items]]\nn = 1\n[[items]]\nn = 2\n[items.sub]\nk = 3\n"sv}
               .load<toml::value>();
  CHECK(v["items"].is_array());
  CHECK(v["items"].size() == 2);
  CHECK(v["items"][0]["n"] == 1);
  CHECK(v["items"][1]["n"] == 2);
  CHECK(v["items"][1]["sub"]["k"] == 3);

  // And comes back out as [[items]] rather than as an inline array of inline
  // tables.
  CHECK(dump(v) == "[[items]]\nn = 1\n[[items]]\nn = 2\n[items.sub]\nk = 3");
}

TEST_CASE("reflex::serde::toml: a toml::value writes tables the way a struct does")
{
  // A map iterates in key order, so a leaf named "z" comes after a table named
  // "a" - and a key written after a [header] belongs to that header's table.
  // The three passes are what stop "z = 9" landing in [a].
  auto v = toml::deserializer{"[a]\nn = 1\n"sv}.load<toml::value>();
  v["z"]  = 9;
  CHECK(dump(v) == "z = 9\n[a]\nn = 1");

  CHECK(dump(toml::value{1}) == "1");
  CHECK(dump(toml::value{1.0}) == "1.0");
  CHECK(dump(toml::value{9007199254740993LL}) == "9007199254740993");
}

TEST_CASE("reflex::serde::toml: a var holding null loses its key")
{
  // TOML has no null, so a var holding one is dropped with its key exactly as
  // an empty optional is. Every other skip the table passes make is a
  // compile-time question; this one is not.
  toml::object o;
  o["a"] = 1;
  o["gone"];
  o["b"] = 2;
  CHECK(dump(o) == "a = 1\nb = 2");

  toml::object inner;
  inner["gone"];
  toml::object outer;
  outer["t"] = inner;
  CHECK(dump(outer) == "[t]");
}

TEST_CASE("reflex::serde::toml::deserializer: a table in a value position needs its braces")
{
  // At depth 0 a bare "key = value" run is a document. Below it, the same
  // bytes are a missing brace, and the message says which.
  const std::string message = load_message<doc_conf>("server = host = \"x\"\n");
  CHECK(message.find('{') != std::string::npos);
  check_load_throws<std::vector<ab_doc>>("[a = 1]");
}

// Each entry is written, read back, written again, and the two writes compared.
// The second write is what matters: a reader and a writer that are wrong in the
// same way pass a single round trip.
//
// The first write of every entry below was also parsed by Python's tomllib and
// compared against the source document, which is what pins the meaning; this
// pins the stability.

namespace
{
  // Read on both cursors, write, read that write back, write again.
  void check_round_trip(std::string_view doc)
  {
    both_cursors(doc, [](auto& de) {
      const std::string w1 = dump(de.template load<toml::value>());
      const std::string w2 = dump(toml::deserializer{std::string_view{w1}}.load<toml::value>());
      CHECK(w1 == w2);
    });
  }

  void check_corpus(std::initializer_list<std::string_view> docs)
  {
    for(const std::string_view doc : docs)
    {
      CAPTURE(doc);
      check_round_trip(doc);
    }
  }

  // The typed leg. A destination type also decides what is written, so a
  // struct that survives text stability may still lose a field.
  template <typename T> void check_typed_round_trip(T const& value)
  {
    const std::string w1 = dump(value);
    CAPTURE(w1);
    const T           back = toml::deserializer{std::string_view{w1}}.template load<T>();
    const std::string w2   = dump(back);
    CHECK(w1 == w2);
    CHECK(back == value);
  }
} // namespace

TEST_CASE("reflex::serde::toml: round trip - every scalar form")
{
  check_corpus({
      R"(a = "basic")",
      R"(a = 'literal')",
      "a = \"\"\"multi\nline\"\"\"",
      "a = '''multi\nliteral'''",
      R"(a = "\b\t\n\f\r\"\\")",
      // The 1.1-only escapes. Nothing else in this file emits them either.
      R"(a = "\e\x01\x7f")",
      // A literal multi-byte sequence in the source, which is not an escape and
      // passes through both ways.
      "a = \"caf\xC3\xA9 \xE2\x86\x92\"",
      "a = 0\nb = -17\nc = +99\nd = 9007199254740993\ne = 1_000_000",
      "a = 0xdeadBEEF\nb = 0o755\nc = 0b1010",
      "a = 3.14\nb = -0.0\nc = 5e+22\nd = 1e6\ne = 6.626e-34\nf = 9_224_617.445_991_228",
      "a = inf\nb = -inf\nc = nan",
      "a = true\nb = false",
      // Date-times are strings, so the write is quoted and the re-read is the
      // string it wrote.
      "a = 1979-05-27T07:32:00Z\n"
      "b = 1979-05-27T00:32:00.999999-07:00\n"
      "c = 1979-05-27\n"
      "d = 07:32:00\n"
      "e = 07:32",
  });
}

TEST_CASE("reflex::serde::toml: round trip - tables and arrays of tables")
{
  check_corpus({
      "[a]\nx = 1\n[a.b]\ny = 2\n[a.b.c]\nz = 3",
      // A header jumping back up a level, which is legal and reorders the write.
      "x = 1\n[a]\ny = 2\n[b]\nz = 3\n[a.c]\nw = 4",
      "[[fruit]]\n"
      "name = \"apple\"\n"
      "[fruit.physical]\n"
      "color = \"red\"\n"
      "[[fruit.variety]]\n"
      "name = \"red delicious\"\n"
      "[[fruit]]\n"
      "name = \"banana\"\n"
      "[[fruit.variety]]\n"
      "name = \"plantain\"",
      "a.b.c = 1\na.b.d = 2\ne = 3",
      // An inline table is a table, so the write is a header section.
      "a = { b = 1, c = { d = 2 } }",
      "a = [1, 2, 3]",
      "a = [[1, 2], [3]]",
      "a = [1, \"two\", true, 4.5]",
      // An array whose elements are all tables is an array of tables on the way
      // out, whatever it looked like on the way in.
      "a = [{ x = 1 }, { y = 2 }]",
      "[[a]]\n[[a]]",
  });
}

TEST_CASE("reflex::serde::toml: round trip - the shapes with nothing in them")
{
  check_corpus({
      "[empty]",
      "a = []",
      "a = {}",
      "[a]\n[a.b]",
      "arr = []\ntbl = {}\n[section]",
  });
}

TEST_CASE("reflex::serde::toml: round trip - keys")
{
  check_corpus({
      "bare-key_1 = 1\nUPPER = 2\n123 = 3",
      R"("quoted key" = 1)"
      "\n"
      R"("dotted.key" = 2)"
      "\n"
      R"('literal key' = 3)",
      // A bare key is ASCII-only in 1.1.0, so a non-ASCII key is quoted both
      // ways.
      "\"\xD0\xBA\xD0\xBB\xD1\x8E\xD1\x87\" = 1\n'\xD0\xBA" "2' = 2",
      R"("" = 1)",
      R"("a\tb" = 1)",
      "[\"quoted.section\"]\nx = 1",
      "[a.\"b.c\".d]\nx = 1",
  });
}

TEST_CASE("reflex::serde::toml: round trip - trivia and line endings")
{
  check_corpus({
      "# only a comment\n\n#   another\n",
      "",
      "a = 1 # trailing comment\n# leading\nb = 2",
      // No trailing newline, and one.
      "a = 1",
      "a = 1\n",
      "a = 1\r\n[b]\r\nc = 2\r\n",
      "\r\n\r\na = 1\r\n",
      "  a = 1\n\t[b]\n  c = 2",
  });
}

namespace
{
  struct rt_leaf
  {
    int  n;
    bool operator==(rt_leaf const&) const = default;
  };
  struct rt_mid
  {
    int     v;
    rt_leaf leaf;
    bool    operator==(rt_mid const&) const = default;
  };
  struct rt_deep
  {
    rt_mid a;
    bool   operator==(rt_deep const&) const = default;
  };
  struct rt_opt
  {
    int                        a;
    std::optional<int>         b;
    std::optional<std::string> c;
    bool                       operator==(rt_opt const&) const = default;
  };
  struct rt_collections
  {
    std::vector<int>                   nums;
    std::vector<rt_leaf>               items;
    std::map<std::string, rt_leaf>     tables;
    std::map<std::string, std::string> leaves;
    bool operator==(rt_collections const&) const = default;
  };
} // namespace

TEST_CASE("reflex::serde::toml: round trip - typed destinations")
{
  check_typed_round_trip(rt_deep{{1, {2}}});
  check_typed_round_trip(rt_collections{{1, 2}, {{3}, {4}}, {{"x", {5}}}, {{"y", "z"}}});
  check_typed_round_trip(rt_collections{});
}

TEST_CASE("reflex::serde::toml: round trip - an absent optional stays absent")
{
  // The key is gone from the write and comes back absent, rather than coming
  // back engaged with a default.
  const rt_opt absent{1, std::nullopt, std::nullopt};
  CHECK(dump(absent) == "a = 1");
  check_typed_round_trip(absent);

  check_typed_round_trip(rt_opt{1, 2, "three"});
  check_typed_round_trip(rt_opt{1, std::nullopt, "three"});
}
