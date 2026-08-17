#include <doctest/doctest.h>
import reflex.serde.toml;
import std;
using namespace reflex;
using namespace reflex::serde;
using namespace std::literals;

namespace
{
  template <typename T> std::string dump(T const& v)
  {
    std::string      out;
    toml::serializer ser{out};
    ser.dump(v);
    return out;
  }
}

struct Tls
{
  bool enabled;
  int  port;

  bool operator==(Tls const&) const = default;
};

struct Server
{
  std::string      host;
  std::vector<int> workers;
  Tls              tls;

  bool operator==(Server const&) const = default;
};

TEST_CASE("README toml: quick start snippet")
{
  Server s{"localhost", {1, 2}, {true, 8443}};
  CHECK(
      dump(s)
      == "host = \"localhost\"\n"
         "workers = [1, 2]\n"
         "[tls]\n"
         "enabled = true\n"
         "port = 8443");
  CHECK(toml::deserializer{dump(s)}.load<Server>() == s);
}

struct Probe
{
  std::string host;
  Tls         tls;
  int         port;
};

TEST_CASE("README toml: a key declared after a table is still written first")
{
  CHECK(
      dump(Probe{"h", {true, 8443}, 8080})
      == "host = \"h\"\n"
         "port = 8080\n"
         "[tls]\n"
         "enabled = true\n"
         "port = 8443");
}

struct MapDoc
{
  std::map<std::string, Tls> m;
};

TEST_CASE("README toml: a map member gets its own header")
{
  CHECK(
      dump(MapDoc{{{"a", {true, 1}}, {"has space", {false, 2}}}})
      == "[m]\n"
         "[m.a]\n"
         "enabled = true\n"
         "port = 1\n"
         "[m.\"has space\"]\n"
         "enabled = false\n"
         "port = 2");
}

struct Empty
{};

struct EmptyDoc
{
  int                    a;
  Empty                  child;
  std::vector<Tls>       items;
  std::optional<int>     absent;
};

TEST_CASE("README toml: an empty aggregate keeps its header, an empty vector of tables writes nothing")
{
  CHECK(dump(EmptyDoc{1, {}, {}, std::nullopt}) == "a = 1\n[child]");
}

TEST_CASE("README toml: an array element has no key to omit")
{
  CHECK(dump(std::vector<std::optional<int>>{1, 2}) == "[1, 2]");
  CHECK_THROWS(dump(std::vector<std::optional<int>>{1, std::nullopt}));
}

TEST_CASE("README toml: string form snippet")
{
  CHECK(dump(R"(C:\path\to)"s) == R"('C:\path\to')");
  CHECK(dump(R"(a "quoted" word)"s) == R"('a "quoted" word')");
  CHECK(dump("it's plain"s) == R"("it's plain")");
  CHECK(dump(R"(it's a \ mess)"s) == R"("it's a \\ mess")");
  CHECK(dump("C:\\a\tb"s) == R"("C:\\a\tb")");
}

TEST_CASE("README toml: control byte snippet")
{
  CHECK(dump("\x1B"s) == R"("\e")");
  CHECK(dump("\x01"s) == R"("\x01")");
  CHECK(dump("\x7F"s) == R"("\x7f")");
  CHECK(dump("\b\t\n\f\r"s) == R"("\b\t\n\f\r")");
  CHECK(dump("\0\a\v"s) == R"("\x00\x07\x0b")");
}

TEST_CASE("README toml: all four string forms are read, neither multi-line form is written")
{
  CHECK(toml::deserializer{R"("basic")"sv}.load<std::string>() == "basic");
  CHECK(toml::deserializer{R"('literal')"sv}.load<std::string>() == "literal");
  CHECK(toml::deserializer{"\"\"\"a\nb\"\"\""sv}.load<std::string>() == "a\nb");
  CHECK(toml::deserializer{"'''a\nb'''"sv}.load<std::string>() == "a\nb");
  // A trailing backslash deletes the break and the whitespace after it.
  CHECK(toml::deserializer{"\"\"\"a\\\n   b\"\"\""sv}.load<std::string>() == "ab");
  CHECK(dump("a\nb"s) == R"("a\nb")");
}

struct RenameDoc
{
  int                                   plain;
  [[= serde::rename{"with space"}]] int spaced;
  [[= serde::rename{"my table"}]] Tls   t;
};

TEST_CASE("README toml: renamed key snippet")
{
  CHECK(
      dump(RenameDoc{1, 2, {true, 3}})
      == "plain = 1\n"
         "\"with space\" = 2\n"
         "[\"my table\"]\n"
         "enabled = true\n"
         "port = 3");
}

TEST_CASE("README toml: number snippet")
{
  CHECK(dump(1) == "1");
  CHECK(dump(1.0) == "1.0");
  CHECK(dump(1e30) == "1e+30");
  CHECK(dump(std::int64_t{9007199254740993}) == "9007199254740993");
}

TEST_CASE("README toml: an integer survives a document round trip at full width")
{
  static_assert(std::same_as<toml::integer, std::int64_t>);
  static_assert(std::same_as<toml::number, double>);
  static_assert(std::same_as<toml::value, poly::var<std::int64_t, double, bool, std::string>>);

  auto v = toml::deserializer{"n = 9007199254740993\nf = 1.0\n"sv}.load<toml::value>();
  CHECK(std::holds_alternative<toml::integer>(v["n"]));
  CHECK(std::holds_alternative<toml::number>(v["f"]));
  CHECK(dump(v) == "f = 1.0\nn = 9007199254740993");
}

TEST_CASE("README toml: a document loaded as a toml::value keeps the table layout")
{
  const auto doc = "[[items]]\nn = 1\n[[items]]\nn = 2\n[items.sub]\nk = 3"sv;
  CHECK(dump(toml::deserializer{doc}.load<toml::value>()) == doc);
}

struct DateDoc
{
  std::string d;
};

TEST_CASE("README toml: a date-time is a string and comes back quoted")
{
  const auto in = toml::deserializer{"d = 1979-05-27T07:32:00Z"sv}.load<DateDoc>();
  CHECK(in.d == "1979-05-27T07:32:00Z");
  CHECK(dump(in) == "d = \"1979-05-27T07:32:00Z\"");
}

TEST_CASE("README toml: an escape above U+007F throws, a literal sequence does not")
{
  CHECK_THROWS(toml::deserializer{R"("\u00e9")"sv}.load<std::string>());
  CHECK_THROWS(toml::deserializer{R"("\xff")"sv}.load<std::string>());
  CHECK(toml::deserializer{"\"caf\xC3\xA9\""sv}.load<std::string>() == "caf\xC3\xA9");
}

TEST_CASE("README toml: a key path is capped at 32 segments")
{
  static_assert(serde::max_key_depth == 32);

  const auto path = [](int n) {
    std::string s;
    for(int i = 0; i < n; ++i)
    {
      s += (i == 0 ? "a" : ".a");
    }
    return s + " = 1";
  };
  CHECK_NOTHROW(toml::deserializer{std::string_view{path(32)}}.load<toml::value>());
  CHECK_THROWS(toml::deserializer{std::string_view{path(33)}}.load<toml::value>());
}

struct Pair
{
  int a;
  int b;
};

TEST_CASE("README toml: a destination too small for the document is an error")
{
  CHECK_THROWS(
      toml::deserializer{"[[items]]\na = 1\nb = 2\n[[items]]\na = 3\nb = 4\n"
                         "[[items]]\na = 5\nb = 6"sv}
          .load<std::array<Pair, 2>>());
  CHECK_THROWS(toml::deserializer{"[1, 2, 3]"sv}.load<std::array<int, 2>>());
}

struct Known
{
  int a;
};

TEST_CASE("README toml: a header naming a member the destination lacks throws with nothing under it")
{
  CHECK_THROWS(toml::deserializer{"a = 1\n[nope]"sv}.load<Known>());
}

struct VarMember
{
  int         a;
  toml::value v;
};

TEST_CASE("README toml: a toml::value member of an aggregate is written inline")
{
  toml::object o;
  o["x"] = 1;
  CHECK(dump(VarMember{1, toml::value{o}}) == "a = 1\nv = { x = 1 }");
}

TEST_CASE("README toml: the reader refuses a document that defines the same thing twice")
{
  CHECK_THROWS(toml::deserializer{"[a]\nn = 1\nn = 2"sv}.load<toml::value>());
  CHECK_THROWS(toml::deserializer{"[a]\n[a.b]\n[a]"sv}.load<toml::value>());
  CHECK_THROWS(toml::deserializer{"a = 1\n[a]"sv}.load<toml::value>());
  CHECK_THROWS(toml::deserializer{"a = { b = 1 }\n[a.c]"sv}.load<toml::value>());
  CHECK_THROWS(toml::deserializer{"b = { c = 1 }\n[b.c.d]"sv}.load<toml::value>());
  CHECK_THROWS(toml::deserializer{"[[x]]\n[x]"sv}.load<toml::value>());
  CHECK_THROWS(toml::deserializer{"[x]\n[[x]]"sv}.load<toml::value>());
  CHECK_THROWS(toml::deserializer{"x = []\n[[x]]"sv}.load<toml::value>());
}

TEST_CASE("README toml: 1.1 forms a 1.0 parser rejects are accepted on the way in")
{
  CHECK(toml::deserializer{R"("\e")"sv}.load<std::string>() == "\x1B");
  CHECK(toml::deserializer{R"("\x41")"sv}.load<std::string>() == "A");
  CHECK(toml::deserializer{"{ a = 1,\n  b = 2,\n}"sv}.load<Pair>().b == 2);
  CHECK(toml::deserializer{"[1,\n 2,\n]"sv}.load<std::vector<int>>().size() == 2);
  CHECK(toml::deserializer{"07:32"sv}.load<std::string>() == "07:32");
}
