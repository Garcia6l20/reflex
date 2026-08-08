#include <doctest/doctest.h>
import reflex.serde.yaml;
import std;
using namespace reflex;
using namespace reflex::serde;
using namespace std::literals;

namespace
{
  template <typename T> std::string dump(T const& v)
  {
    std::string      out;
    yaml::serializer ser{out};
    ser.dump(v);
    return out;
  }
}

struct Server
{
  std::string      host;
  int              port;
  std::vector<int> workers;

  bool operator==(Server const&) const = default;
};

TEST_CASE("README yaml: quick start snippet")
{
  Server s{"localhost", 8080, {1, 2}};
  CHECK(dump(s) == "host: localhost\nport: 8080\nworkers:\n  - 1\n  - 2");
  CHECK(yaml::deserializer{dump(s)}.load<Server>() == s);
}

TEST_CASE("README yaml: quoting snippet")
{
  CHECK(dump("1.2.3-rc1"s) == "1.2.3-rc1");
  CHECK(dump("42"s) == "'42'");
  CHECK(dump("null"s) == "'null'");
  CHECK(dump("yes"s) == "'yes'");
  CHECK(dump("a: b"s) == "'a: b'");
  CHECK(dump("a\nb"s) == "\"a\\nb\"");
}

TEST_CASE("README yaml: load<bool> refuses a 1.1 boolean")
{
  CHECK_THROWS(yaml::deserializer{"yes"sv}.load<bool>());
  CHECK(std::holds_alternative<yaml::string>(yaml::deserializer{"yes"sv}.load<yaml::value>()));
}

TEST_CASE("README yaml: quoting defeats resolution")
{
  CHECK(std::holds_alternative<yaml::number>(yaml::deserializer{"42"sv}.load<yaml::value>()));
  CHECK(std::holds_alternative<yaml::string>(yaml::deserializer{"'42'"sv}.load<yaml::value>()));
}

struct Folded
{
  std::string summary;
  std::string note;

  bool operator==(Folded const&) const = default;
};

TEST_CASE("README yaml: multi-line plain scalar snippet")
{
  const auto doc =
      "summary: a description long enough\n"
      "  that it runs onto a second line\n"
      "note: first paragraph\n"
      "\n"
      "  second paragraph"sv;

  CHECK(
      yaml::deserializer{doc}.load<Folded>()
      == Folded{
          "a description long enough that it runs onto a second line",
          "first paragraph\nsecond paragraph"});
}

TEST_CASE("README yaml: what does and does not end a continuation")
{
  // A mapping shape is ambiguous with a nested block, so it is an error.
  CHECK_THROWS(yaml::deserializer{"key: a\n  b: c"sv}.load<Folded>());
  // '-' and '?' are not ambiguous and fold into the text.
  CHECK(yaml::deserializer{"a\n  - b"sv}.load<std::string>() == "a - b");
  CHECK(yaml::deserializer{"a\n  ? b"sv}.load<std::string>() == "a ? b");
}

// The README says this is a gap rather than a refusal. Pinned so the claim
// cannot quietly go stale in either direction.
TEST_CASE("README yaml: explicit-key syntax is read as a plain scalar, not refused")
{
  CHECK(yaml::deserializer{"? a"sv}.load<std::string>() == "? a");
}
