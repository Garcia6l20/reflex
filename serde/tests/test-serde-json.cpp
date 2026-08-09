#include <doctest/doctest.h>
#include <reflex/const_check.hpp>

import reflex.serde.json;
import serde.tests.types;

import std;

using namespace reflex;
using namespace reflex::serde;
using namespace std::literals;

// The bulk-scan cliff, pinned. Same constant, same shared cursor as xml: an
// in-memory input takes the from_chars fast path, a stream cursor does not.
static_assert(json::deserializer<std::string_view::const_iterator>::bulk_scan);
static_assert(not json::deserializer<std::istreambuf_iterator<char>>::bulk_scan);

#define JSON(...) #__VA_ARGS__

struct[[= serde::naming::camel_case, = derive(Debug)]] S
{
  int                                    int_member;
  std::string                            string_member;
  [[= serde::naming::kebab_case]] double double_member;

  constexpr bool operator==(S const& other) const = default;
};

struct[[= serde::naming::camel_case, = derive(Debug)]] S2
{
  int                         i;
  std::array<char, 5>         chars;
  std::array<std::uint8_t, 5> nums;

  constexpr bool operator==(S2 const& other) const = default;
};

struct[[= serde::naming::camel_case, = derive(Debug)]] S3
{
  std::optional<S>  s;
  std::optional<S2> s2;
  constexpr bool    operator==(S3 const& other) const = default;
};

enum class[[= derive(EnumFlags, Format, Parse)]] FilePermissions
{
  None    = 0,
  Read    = 1 << 0,
  Write   = 1 << 1,
  Execute = 1 << 2
};

using namespace std::string_view_literals;

TEST_CASE("reflex::serde::json::serializer: base types")
{
  std::ostringstream out;
  json::serializer   ser{out};

  using json::null;

  SUBCASE("null")
  {
    ser.dump(null);
    CHECK_EQ(out.str(), "null"sv);
  }

  SUBCASE("string")
  {
    ser.dump("Hello, world!");
    CHECK_EQ(out.str(), "\"Hello, world!\"");
  }

  SUBCASE("number")
  {
    ser.dump(42);
    CHECK_EQ(out.str(), "42");
  }

  SUBCASE("boolean")
  {
    ser.dump(true);
    CHECK_EQ(out.str(), "true");
    out.str("");
    ser.dump(false);
    CHECK_EQ(out.str(), "false");
  }

  SUBCASE("enum")
  {
    ser.dump(Color::Green);
    CHECK_EQ(out.str(), "\"Green\"");
  }

  SUBCASE("enum-flags")
  {
    constexpr auto perms = FilePermissions::Read | FilePermissions::Write;
    ser.dump(perms);
    CHECK_EQ(out.str(), "\"Read|Write\"");
  }

  SUBCASE("char array")
  {
    std::array<char, 5> arr = {'H', 'e', 'l', 'l', 'o'};
    ser.dump(arr);
    std::println("Serialized char array: {}", out.str());
    CHECK_EQ(out.str(), "\"Hello\"");
  }

  SUBCASE("num array")
  {
    std::array<std::uint8_t, 5> arr = {0x01, 0x02, 0x03, 0x04, 0x05};
    ser.dump(arr);
    std::println("Serialized num array: {}", out.str());
    CHECK_EQ(out.str(), "[1,2,3,4,5]");
  }

  SUBCASE("optional")
  {
    std::optional<int> opt = 42;
    ser.dump(opt);
    CHECK_EQ(out.str(), "42");
    out.str("");
    opt = std::nullopt;
    ser.dump(opt);
    CHECK_EQ(out.str(), "null");
  }
}

TEST_CASE("reflex::serde::json::serializer: sequence and map")
{
  std::string      out;
  json::serializer ser{out};
  SUBCASE("sequence")
  {
    std::vector<int> arr = {1, 2, 3};
    ser.dump(arr);
    CHECK_EQ(out, JSON([1,2,3]));
  }

  SUBCASE("map")
  {
    auto obj = std::map<std::string, int>{
        {"a", 1},
        {"b", 2}
    };
    ser.dump(obj);
    CHECK_EQ(out, JSON({"a":1,"b":2}));
  }
}

TEST_CASE("reflex::serde::json::serializer: aggregate")
{
  std::string out;
  out.reserve(128); // avoid reallocations during serialization
  json::serializer ser{out};

  SUBCASE("simple aggregate")
  {
    S s{42, "Hello, world!", 3.14};
    ser.dump(s);
    CHECK_EQ(out, JSON({"intMember":42,"stringMember":"Hello, world!","double-member":3.14}));
  }

  SUBCASE("complex aggregate")
  {
    S2 s2{
        42, {'H', 'e', 'l', 'l', 'o'},
         {1,   2,   3,   4,   5  }
    };
    ser.dump(s2);
    CHECK_EQ(out, JSON({"i":42,"chars":"Hello","nums":[1,2,3,4,5]}));
  }

  SUBCASE("optional aggregate")
  {
    S3 s3{
        S{42, "Hello, world!",           3.14           },
        S2{42, {'H', 'e', 'l', 'l', 'o'}, {1, 2, 3, 4, 5}}
    };
    ser.dump(s3);
    CHECK_EQ(
        out, JSON({"s":{"intMember":42,"stringMember":"Hello, world!","double-member":3.14},"s2":{"i":42,"chars":"Hello","nums":[1,2,3,4,5]}}));
    out.clear();
    s3.s  = std::nullopt;
    s3.s2 = std::nullopt;
    ser.dump(s3);
    CHECK_EQ(out, JSON({"s":null,"s2":null}));
  }
}

TEST_CASE("reflex::serde::json::serializer: nested aggregate")
{
  std::string out;
  out.reserve(128); // avoid reallocations during serialization
  json::serializer ser{out};

  struct[[= serde::naming::camel_case]] Inner
  {
    int int_member;
  };

  struct[[= serde::naming::camel_case]] Outer
  {
    Inner       inner;
    std::string string_member;
  };

  Outer o{{42}, "Hello, world!"};
  ser.dump(o);
  CHECK_EQ(out, JSON({"inner":{"intMember":42},"stringMember":"Hello, world!"}));
}

TEST_CASE("reflex::serde::json::deserializer: base types")
{
  using json::null;
  SUBCASE("null")
  {
    const auto in    = "null"s;
    auto       value = json::deserializer{in}.load<json::null_t>();
    CHECK(value == null);
    CHECK(json::deserializer{in}.load<json::value>().is_null());
  }

  SUBCASE("string")
  {
    const std::string_view in    = JSON("Hello, world!");
    auto                   value = json::deserializer{in}.load<std::string>();
    CHECK(value == "Hello, world!");
    auto var = json::deserializer{in}.load<json::value>();
    CHECK(var.is<json::string>());
    CHECK(var == "Hello, world!");
  }

  SUBCASE("number")
  {
    const std::string_view in    = JSON(42);
    auto                   value = json::deserializer{in}.load<int>();
    CHECK(value == 42);
    auto var = json::deserializer{in}.load<json::value>();
    CHECK(var.is<json::number>());
    CHECK(var == 42);
  }

  SUBCASE("boolean")
  {
    const std::string_view in    = JSON(true);
    auto                   value = json::deserializer{in}.load<bool>();
    CHECK(value == true);
    const std::string_view in_false = JSON(false);
    value                           = json::deserializer{in_false}.load<bool>();
    CHECK(value == false);
  }

  SUBCASE("enum")
  {
    const std::string_view in    = JSON("Green");
    auto                   value = json::deserializer{in}.load<Color>();
    CHECK(value == Color::Green);
  }

  SUBCASE("enum-flags")
  {
    const std::string_view in    = JSON("Read|Write");
    auto                   value = json::deserializer{in}.load<FilePermissions>();
    CHECK((value & FilePermissions::Read) == FilePermissions::Read);
    CHECK((value & FilePermissions::Write) == FilePermissions::Write);
    CHECK((value & FilePermissions::Execute) == FilePermissions::None);
  }

  SUBCASE("char array")
  {
    const std::string_view in    = JSON("Hello");
    auto                   value = json::deserializer{in}.load<std::array<char, 5>>();
    CHECK(value == std::array<char, 5>{'H', 'e', 'l', 'l', 'o'});
  }

  SUBCASE("num array")
  {
    const std::string_view in    = JSON([1,2,3,4,5]);
    auto                   value = json::deserializer{in}.load<std::array<std::uint8_t, 5>>();
    CHECK(value == std::array<std::uint8_t, 5>{1, 2, 3, 4, 5});
  }

  SUBCASE("optional")
  {
    const std::string_view in    = JSON(42);
    auto                   value = json::deserializer{in}.load<std::optional<int>>();
    CHECK(value.has_value());
    CHECK(value.value() == 42);

    const std::string_view in_null = JSON(null);
    value                          = json::deserializer{in_null}.load<std::optional<int>>();
    CHECK(!value.has_value());
  }
}

TEST_CASE("reflex::serde::json::deserializer: sequence and map")
{
  SUBCASE("sequence")
  {
    const std::string_view in    = JSON([1,2,3]);
    const auto             value = json::deserializer{in}.load<json::array>();
    CHECK_EQ(value.size(), 3);
    CHECK_EQ(value[0], 1);
    CHECK_EQ(value[1], 2);
    CHECK_EQ(value[2], 3);
    const auto expected = json::array{1, 2, 3};
    CHECK_EQ(value, expected);
  }

  SUBCASE("map")
  {
    const std::string_view in    = JSON({"a":1,"b":2});
    const auto             value = json::deserializer{in}.load<json::object>();
    std::println("Parsed object: {}", value);
    CHECK_EQ(value.size(), 2);
    CHECK_EQ(value.at("a"), 1);
    CHECK_EQ(value.at("b"), 2);
    const auto expected = json::object{
        {"a", 1},
        {"b", 2}
    };
    CHECK_EQ(value, expected);
  }
}

TEST_CASE("reflex::serde::json::deserializer: aggregate")
{
  static_assert(serde::object_visitable_c<S>);
  static_assert(serde::object_visitable_c<S&>);

  SUBCASE("direct load")
  {
    const std::string_view in =
        JSON({"intMember":42,"stringMember":"Hello, world!","double-member":3.14});
    const auto value = json::deserializer{in}.load<S>();
    CHECK_EQ(value.int_member, 42);
    CHECK_EQ(value.string_member, "Hello, world!");
    CHECK_EQ(value.double_member, 3.14);
  }
  SUBCASE("complex aggregate")
  {
    const std::string_view in    = JSON({"i":42,"chars":"Hello","nums":[1,2,3,4,5]});
    const auto             value = json::deserializer{in}.load<S2>();
    std::println("Parsed S2: {}", value);
    CHECK_EQ(value.i, 42);
    CHECK_EQ(value.chars, std::array<char, 5>{'H', 'e', 'l', 'l', 'o'});
    CHECK_EQ(value.nums, std::array<std::uint8_t, 5>{1, 2, 3, 4, 5});
  }
  SUBCASE("optional aggregate - present")
  {
    const std::string_view in    = JSON({"s":{"intMember":42,"stringMember":"Hello, world!","double-member":3.14},"s2":{"i":42,"chars":"Hello","nums":[1,2,3,4,5]}});
    const auto             value = json::deserializer{in}.load<S3>();
    std::println("Parsed S3: {}", value);
    CHECK(value.s.has_value());
    CHECK(value.s2.has_value());
    CHECK_EQ(value.s->int_member, 42);
    CHECK_EQ(value.s->string_member, "Hello, world!");
    CHECK_EQ(value.s->double_member, 3.14);
    CHECK_EQ(value.s2->i, 42);
    CHECK_EQ(value.s2->chars, std::array<char, 5>{'H', 'e', 'l', 'l', 'o'});
    CHECK_EQ(value.s2->nums, std::array<std::uint8_t, 5>{1, 2, 3, 4, 5});
  }
  SUBCASE("optional aggregate - absent")
  {
    const std::string_view in    = JSON({"s":null,"s2":null});
    const auto             value = json::deserializer{in}.load<S3>();
    std::println("Parsed S3: {}", value);
    CHECK(!value.s.has_value());
    CHECK(!value.s2.has_value());
  }
  SUBCASE("optional aggregate - absent 2")
  {
    const std::string_view in    = JSON({});
    const auto             value = json::deserializer{in}.load<S3>();
    std::println("Parsed S3: {}", value);
    CHECK(!value.s.has_value());
    CHECK(!value.s2.has_value());
  }
  SUBCASE("a member reads only under the name it is written under")
  {
    // S is camelCase, so int_member serializes as intMember. The C++ identifier
    // is not a second name the reader also accepts.
    const std::string_view serialized = JSON({"intMember":42});
    const std::string_view identifier  = JSON({"int_member":42});
    CHECK_EQ(json::deserializer{serialized}.load<S>().int_member, 42);
    CHECK_THROWS(json::deserializer{identifier}.load<S>());
  }
  SUBCASE("a dotted key names one member, it is not a path")
  {
    // A dot is an ordinary character in a JSON key. Reading it as a path would
    // let a document write a member it never named.
    const std::string_view in = JSON({"s.intMember":42});
    CHECK_THROWS(json::deserializer{in}.load<S3>());
  }
}

using custom_var1 = poly::var<json::string, json::number, json::boolean, S>;

TEST_CASE("reflex::serde::json::deserializer: custom var")
{
  std::string      out;
  json::serializer ser{out};

  SUBCASE("custom var")
  {
    serialize(
        ser, custom_var1{
                 {"test", S{42, "Hello, world!", 3.14}}
    });
    std::println("Serialized: {}", out);
    const auto value = json::deserializer{out}.load<custom_var1>();
    std::println("Deserialized: {}", value);
    CHECK_EQ(value.at("test")->as<S>().int_member, 42);
    CHECK_EQ(value.at("test")->as<S>().string_member, "Hello, world!");
    CHECK_EQ(value.at("test")->as<S>().double_member, 3.14);
  }
}

TEST_CASE("reflex::core::json file roundtrip")
{
  const std::filesystem::path json_path = "test.json";
  const S                     expected  = {42, "Hello, world!", 3.14};
  {
    std::ofstream    out_file{json_path};
    json::serializer ser{out_file};
    ser.dump(expected);
  }

  {
    std::ifstream in_file{json_path};
    const auto    value = json::deserializer{in_file}.load<S>();
    CHECK_EQ(value, expected);
  }

  {
    // the same file through the contiguous path: one deduction guide, no
    // json-specific entry point, and bulk_scan stays on
    serde::mmap_input_stream in{json_path};
    auto                     de = json::deserializer{in};
    static_assert(std::same_as<decltype(de), json::deserializer<const char*>>);
    CHECK_EQ(de.load<S>(), expected);
  }

  std::filesystem::remove(json_path);
}

struct test_userdefined_type
{
  int         a;
  double      b;
  std::string c;

  constexpr bool operator==(test_userdefined_type const& other) const = default;
};

template <typename OutputIt>
auto tag_invoke(
    tag_t<serde::serialize>,
    serde::json::serializer<OutputIt>&                         ser,
    [[maybe_unused]] test_userdefined_type const& value) -> OutputIt
{
  return std::format_to(ser.out(), "user-defined");
}

template <typename InputIt>
auto tag_invoke(
    tag_t<serde::deserialize>,
    serde::json::deserializer<InputIt>&,
    std::type_identity<test_userdefined_type>)
{
  return test_userdefined_type{42, 3.14, "Hello, world!"};
}

TEST_CASE("reflex::serde::json: user-defined type roundtrip")
{
  const test_userdefined_type expected = {42, 3.14, "Hello, world!"};
  std::string                 out;
  {
    json::serializer ser{out};
    ser.dump(expected);
  }
  std::println("Serialized: {}", out);
  CHECK_EQ(out, "user-defined"sv);
  const auto value = json::deserializer{out}.load<test_userdefined_type>();
  CHECK_EQ(value, expected);
}


struct test_userdefined_type2
{
  int         a;
  double      b;
  std::string c;

  constexpr bool operator==(test_userdefined_type2 const& other) const = default;
};

template <typename T>
concept user_defined2 = std::same_as<T, test_userdefined_type2>;

template <typename OutputIt, user_defined2 T>
auto tag_invoke(
    tag_t<serde::serialize>,
    serde::json::serializer<OutputIt>&                         ser,
    [[maybe_unused]] T const& value) -> OutputIt
{
  return std::format_to(ser.out(), "user-defined2");
}

  

template <typename InputIt, user_defined2 T>
auto tag_invoke(
    tag_t<serde::deserialize>,
    serde::json::deserializer<InputIt>&,
    std::type_identity<T>)
{
  return T{42, 3.14, "Hello, world2!"};
}

TEST_CASE("reflex::serde::json: user-defined type roundtrip 2")
{
  const test_userdefined_type2 expected = {42, 3.14, "Hello, world2!"};
  std::string                  out;
  {
    json::serializer ser{out};
    ser.dump(expected);
  }
  std::println("Serialized: {}", out);
  CHECK_EQ(out, "user-defined2"sv);
  const auto value = json::deserializer{out}.load<test_userdefined_type2>();
  CHECK_EQ(value, expected);
}

// User-defined type roundtrippable as a JSON number, nested inside a plain
// aggregate to verify the override applies through the generic aggregate path.
struct nested_custom
{
  int            v;
  constexpr bool operator==(nested_custom const& other) const = default;
};

template <typename OutputIt>
auto tag_invoke(
    tag_t<serde::serialize>,
    serde::json::serializer<OutputIt>& ser,
    nested_custom const&               value) -> OutputIt
{
  return std::format_to(ser.out(), "{}", value.v * 2);
}

template <typename InputIt>
auto tag_invoke(
    tag_t<serde::deserialize>,
    serde::json::deserializer<InputIt>& de,
    std::type_identity<nested_custom>)
{
  return nested_custom{de.template load<int>() / 2};
}

struct[[= serde::naming::camel_case]] custom_holder
{
  nested_custom  inner;
  int            other;
  constexpr bool operator==(custom_holder const& o) const = default;
};

TEST_CASE("reflex::serde::json: user-defined type nested in aggregate")
{
  const custom_holder expected = {nested_custom{21}, 7};
  std::string         out;
  {
    json::serializer ser{out};
    ser.dump(expected);
  }
  std::println("Serialized: {}", out);
  CHECK_EQ(out, JSON({"inner":42,"other":7}));
  const auto value = json::deserializer{out}.load<custom_holder>();
  CHECK_EQ(value, expected);
}

namespace
{
  std::string json_dump(auto const& value)
  {
    std::string      out;
    json::serializer ser{out};
    ser.dump(value);
    return out;
  }

  struct[[= derive(Debug)]] Escaped
  {
    std::string    text;
    int            n;
    constexpr bool operator==(Escaped const&) const = default;
  };
} // namespace

TEST_CASE("reflex::serde::json: string escaping")
{
  SUBCASE("the seven named escapes")
  {
    CHECK_EQ(json_dump("a\"b"s), "\"a\\\"b\"");
    CHECK_EQ(json_dump("a\\b"s), "\"a\\\\b\"");
    CHECK_EQ(json_dump("a\bb"s), "\"a\\bb\"");
    CHECK_EQ(json_dump("a\fb"s), "\"a\\fb\"");
    CHECK_EQ(json_dump("a\nb"s), "\"a\\nb\"");
    CHECK_EQ(json_dump("a\rb"s), "\"a\\rb\"");
    CHECK_EQ(json_dump("a\tb"s), "\"a\\tb\"");
  }

  SUBCASE("the unnamed control characters use the \\u00XX form")
  {
    CHECK_EQ(json_dump(std::string(1, '\0')), "\"\\u0000\"");
    CHECK_EQ(json_dump("\x01"s), "\"\\u0001\"");
    CHECK_EQ(json_dump("\x1f"s), "\"\\u001f\"");
  }

  SUBCASE("solidus is not escaped on output but is accepted on input")
  {
    CHECK_EQ(json_dump("a/b"s), "\"a/b\"");
    CHECK_EQ(json::deserializer{"\"a\\/b\""sv}.load<std::string>(), "a/b");
  }

  SUBCASE("every control character round-trips and none survives literally")
  {
    for(int c = 0; c < 0x20; ++c)
    {
      CAPTURE(c);
      const std::string original{'x', static_cast<char>(c), 'y'};
      const std::string encoded = json_dump(original);
      CHECK_EQ(encoded.find(static_cast<char>(c)), std::string::npos);
      CHECK_EQ(json::deserializer{encoded}.load<std::string>(), original);
    }
  }

  SUBCASE("edge shapes")
  {
    CHECK_EQ(json_dump(""s), "\"\"");
    CHECK_EQ(json_dump("\"\"\""s), "\"\\\"\\\"\\\"\"");
    CHECK_EQ(json_dump("ends with\\"s), "\"ends with\\\\\"");
    CHECK_EQ(json::deserializer{json_dump("ends with\\"s)}.load<std::string>(), "ends with\\");
    CHECK_EQ(json::deserializer{json_dump("\"\"\""s)}.load<std::string>(), "\"\"\"");
  }

  SUBCASE("UTF-8 passes through unescaped")
  {
    // two-, three- and four-byte sequences
    for(const auto& original : {"e\u00e9e"s, "e\u20ache"s, "e\U0001F600e"s})
    {
      CAPTURE(original);
      const std::string encoded = json_dump(original);
      CHECK_EQ(encoded, "\"" + original + "\"");
      CHECK_EQ(json::deserializer{encoded}.load<std::string>(), original);
    }
  }

  SUBCASE("char values are escaped too")
  {
    CHECK_EQ(json_dump('"'), "\"\\\"\"");
    CHECK_EQ(json_dump('\\'), "\"\\\\\"");
    CHECK_EQ(json_dump('\n'), "\"\\n\"");
    CHECK_EQ(json_dump('\x01'), "\"\\u0001\"");
    CHECK_EQ(json_dump('a'), "\"a\"");
  }

  SUBCASE("map keys are escaped")
  {
    const auto m = std::map<std::string, int>{
        {"a\"b", 1}
    };
    CHECK_EQ(json_dump(m), "{\"a\\\"b\":1}");
  }

  SUBCASE("the \\u00XX subset below 0x80 is decoded, anything above still throws")
  {
    CHECK_EQ(json::deserializer{"\"\\u0041\""sv}.load<std::string>(), "A");
    CHECK_EQ(json::deserializer{"\"\\u0009\""sv}.load<std::string>(), "\t");
    CHECK_EQ(json::deserializer{"\"\\u007f\""sv}.load<std::string>(), "\x7f");
    CHECK_THROWS(json::deserializer{"\"\\u00e9\""sv}.load<std::string>());
    CHECK_THROWS(json::deserializer{"\"\\ud83d\""sv}.load<std::string>());
    CHECK_THROWS(json::deserializer{"\"\\u00zz\""sv}.load<std::string>());
  }

  SUBCASE("an aggregate carrying every escapable class round-trips")
  {
    const Escaped expected{"quote \" backslash \\ newline \n tab \t bell \x07 done", 7};
    const auto    encoded = json_dump(expected);
    CHECK_EQ(json::deserializer{encoded}.load<Escaped>(), expected);
  }
}

namespace
{
  // No char member: the serializer has a char overload but the deserializer has
  // none, so a char field cannot round-trip. Pre-existing, and the char
  // serialize path is covered by the escaping test case above.
  struct[[= derive(Debug)]] Scalars
  {
    double         d;
    std::int64_t   i;
    std::string    s;
    constexpr bool operator==(Scalars const&) const = default;
  };

  struct[[= derive(Debug)]] Empties
  {
    std::vector<int>       arr;
    std::map<std::string, int> obj;
    std::optional<int>     opt;
    constexpr bool         operator==(Empties const&) const = default;
  };

  // A rename that needs no escaping still has to work: it is the same path the
  // static_assert in detail::quoted_key guards.
  struct[[= derive(Debug)]] Renamed
  {
    [[= serde::rename{"a-weird/name:with punctuation"}]] int value;
    constexpr bool operator==(Renamed const&) const = default;
  };

  struct[[= derive(Debug)]] NonFinite
  {
    double value;
  };

} // namespace

TEST_CASE("reflex::serde::json::serializer: scalar rendering is to_chars")
{
  SUBCASE("explicit values")
  {
    CHECK_EQ(json_dump(3.14159), "3.14159");
    CHECK_EQ(json_dump(1e+300), "1e+300");
    CHECK_EQ(json_dump(-0.0), "-0");
    CHECK_EQ(json_dump(std::numeric_limits<std::int64_t>::min()), "-9223372036854775808");
    CHECK_EQ(json_dump(std::numeric_limits<std::uint64_t>::max()), "18446744073709551615");
    CHECK_EQ(json_dump(0), "0");
  }

  SUBCASE("to_chars renders what format(\"{}\") rendered")
  {
    for(const double d : {3.14159, 1e+300, -0.0, 0.0, 1e-300, 1e16, 1e17, 2.2250738585072014e-308})
    {
      CAPTURE(d);
      CHECK_EQ(json_dump(d), std::format("{}", d));
    }
    for(const std::int64_t i :
        {std::int64_t{0}, std::int64_t{-1}, std::numeric_limits<std::int64_t>::min()})
    {
      CAPTURE(i);
      CHECK_EQ(json_dump(i), std::format("{}", i));
    }
  }

  SUBCASE("infinities and NaN are rejected")
  {
    CHECK_THROWS(json_dump(std::numeric_limits<double>::infinity()));
    CHECK_THROWS(json_dump(-std::numeric_limits<double>::infinity()));
    CHECK_THROWS(json_dump(std::numeric_limits<double>::quiet_NaN()));

    // A member is reached through the same overload, so a whole aggregate is
    // rejected rather than half written.
    CHECK_THROWS(json_dump(NonFinite{std::numeric_limits<double>::infinity()}));

    // Finite extremes still render, so the guard costs nothing at the edges.
    CHECK_EQ(json_dump(std::numeric_limits<double>::max()), "1.7976931348623157e+308");
  }

  SUBCASE("literals and structure")
  {
    CHECK_EQ(json_dump(true), "true");
    CHECK_EQ(json_dump(false), "false");
    CHECK_EQ(json_dump(json::null), "null");
    CHECK_EQ(json_dump(std::optional<int>{}), "null");
    CHECK_EQ(json_dump(std::optional<int>{7}), "7");
    CHECK_EQ(json_dump(std::vector<int>{}), "[]");
    CHECK_EQ(json_dump(std::vector<int>{1}), "[1]");
    CHECK_EQ(json_dump(std::map<std::string, int>{}), "{}");
  }

  SUBCASE("an aggregate of every scalar kind round-trips")
  {
    const Scalars expected{3.14159, std::numeric_limits<std::int64_t>::min(), ""};
    const auto    encoded = json_dump(expected);
    CHECK_EQ(encoded, "{\"d\":3.14159,\"i\":-9223372036854775808,\"s\":\"\"}");
    CHECK_EQ(json::deserializer{encoded}.load<Scalars>(), expected);
  }

  SUBCASE("empty containers inside an aggregate")
  {
    const Empties expected{};
    const auto    encoded = json_dump(expected);
    CHECK_EQ(encoded, "{\"arr\":[],\"obj\":{},\"opt\":null}");
  }

  SUBCASE("a rename needing no escape still produces its key token")
  {
    const Renamed expected{42};
    const auto    encoded = json_dump(expected);
    CHECK_EQ(encoded, "{\"a-weird/name:with punctuation\":42}");
    CHECK_EQ(json::deserializer{encoded}.load<Renamed>(), expected);
  }

  // A rename carries the only arbitrary string that becomes a serialized name,
  // so it is the one place these characters can enter. Rejection is a build
  // failure, which is why it is pinned here rather than with a CHECK.
  consteval {
    REFLEX_CONSTEVAL_NOTHROW(rename{"a-weird/name:with punctuation"});
    REFLEX_CONSTEVAL_THROWS(rename{"outer.inner"});
    REFLEX_CONSTEVAL_THROWS(rename{"say \"hi\""});
    REFLEX_CONSTEVAL_THROWS(rename{"back\\slash"});
    REFLEX_CONSTEVAL_THROWS(rename{"nl\n"});
  }
}

namespace
{
  // Reads through the non-contiguous cursor, which has bulk_scan off. Every
  // string result must match the contiguous path byte for byte.
  template <typename T> T load_streaming(std::string_view text)
  {
    std::istringstream in{std::string{text}};
    return json::deserializer{in}.template load<T>();
  }

  template <typename T> T load_contiguous(std::string_view text)
  {
    return json::deserializer{text}.template load<T>();
  }
} // namespace

TEST_CASE("reflex::serde::json::deserializer: string bodies")
{
  // The pair that makes a naive bound wrong: the first '"' in the input is not
  // the terminator once a backslash precedes it.
  SUBCASE("an escaped quote is not the terminator")
  {
    CHECK_EQ(load_contiguous<std::string>("\"a\\\"b\""), "a\"b");
    CHECK_EQ(load_streaming<std::string>("\"a\\\"b\""), "a\"b");
  }

  SUBCASE("a trailing escaped backslash is the last body byte")
  {
    CHECK_EQ(load_contiguous<std::string>("\"a\\\\\""), "a\\");
    CHECK_EQ(load_streaming<std::string>("\"a\\\\\""), "a\\");
    CHECK_EQ(load_contiguous<std::string>("\"\\\\\\\\\""), "\\\\");
  }

  SUBCASE("shapes")
  {
    CHECK_EQ(load_contiguous<std::string>("\"\""), "");
    CHECK_EQ(load_contiguous<std::string>("\"a\""), "a");
    CHECK_EQ(load_contiguous<std::string>("\"\\n\\t\\r\\b\\f\""), "\n\t\r\b\f");
    CHECK_EQ(load_contiguous<std::string>("\"\\\"\\\"\""), "\"\"");
    // a raw newline inside a string is accepted today and must stay accepted
    CHECK_EQ(load_contiguous<std::string>("\"a\nb\""), "a\nb");
    CHECK_EQ(load_streaming<std::string>("\"a\nb\""), "a\nb");
  }

  SUBCASE("truncated input throws rather than reading past the end")
  {
    CHECK_THROWS(load_contiguous<std::string>("\"unterminated"));
    CHECK_THROWS(load_streaming<std::string>("\"unterminated"));
    // ends exactly at EOF with a dangling escape
    CHECK_THROWS(load_contiguous<std::string>("\"abc\\"));
    CHECK_THROWS(load_streaming<std::string>("\"abc\\"));
    CHECK_THROWS(load_contiguous<std::string>("\""));
    CHECK_THROWS(load_contiguous<std::string>(""));
    CHECK_THROWS(load_contiguous<std::string>("\"\\u00"));
  }

  SUBCASE("both cursors agree on a long mixed body")
  {
    std::string body;
    for(int i = 0; i < 200; ++i)
    {
      body += "plain";
      body += "\\\"";
      body += "more";
      body += "\\\\";
      body += "\\n";
    }
    const std::string encoded = "\"" + body + "\"";
    const auto        a       = load_contiguous<std::string>(encoded);
    const auto        b       = load_streaming<std::string>(encoded);
    CHECK_EQ(a, b);
    CHECK_EQ(a.size(), 200u * (5 + 1 + 4 + 1 + 1));
    // and it re-serializes to what it came from
    CHECK_EQ(json_dump(a), encoded);
  }

  SUBCASE("a fixed-capacity target still bounds-checks")
  {
    CHECK_EQ(std::string_view{load_contiguous<heapless::string<8>>("\"abc\"")}, "abc"sv);
    CHECK_EQ(std::string_view{load_contiguous<heapless::string<8>>("\"a\\nc\"")}, "a\nc"sv);
    CHECK_THROWS(load_contiguous<heapless::string<4>>("\"abcdefghij\""));
    CHECK_THROWS(load_contiguous<heapless::string<4>>("\"ab\\ncdefghij\""));
    CHECK_THROWS(load_streaming<heapless::string<4>>("\"abcdefghij\""));
  }

  SUBCASE("keys and values with escapes inside an object")
  {
    const auto v = load_contiguous<json::object>(
        "{\"a\\\"b\":\"c\\\\d\",\"plain\":\"\\u0041\"}");
    REQUIRE_EQ(v.size(), 2u);
    CHECK_EQ(v.at("a\"b"), "c\\d");
    CHECK_EQ(v.at("plain"), "A");
  }
}

// The three shapes of string destination the reader accepts, side by side.
// heapless::string grows into storage it owns, std::array<char, N> is filled
// through its own iterators, std::string does both. All three own the bytes
// they are written into, which is the property the reader actually needs.
struct StringSinks
{
  heapless::string<8> small;
  std::array<char, 5> chars;
  std::string         owned;

  bool operator==(StringSinks const& other) const = default;
};

static_assert(serde::detail::string_sink_c<heapless::string<8>>);
static_assert(serde::detail::string_sink_c<std::array<char, 5>>);
static_assert(serde::detail::string_sink_c<std::string>);

// Which side each one comes in on. heapless::string grows through
// inplace_vector's push_back, so it is not on the fixed-capacity path even
// though its capacity is fixed. std::array<char, N> is the one that is.
static_assert(serde::detail::growable_string_sink_c<heapless::string<8>>);
static_assert(not serde::detail::fixed_string_sink_c<std::string_view>);
static_assert(serde::detail::fixed_string_sink_c<std::array<char, 5>>);
static_assert(not serde::detail::growable_string_sink_c<std::array<char, 5>>);

// A std::string_view owns nothing, so it is not a sink. It is the borrowed
// destination instead, and reads a view of the input. char const* is neither: it
// cannot be built from a pointer and a length, and a run of the input carries no
// terminator. It is a compile error naming the type:
//
//   error: static assertion failed: const char* cannot be a JSON string
//   destination: it does not own writable storage (expected std::string,
//   reflex::heapless::string<N> or std::array<char, N>)
static_assert(not serde::detail::string_sink_c<std::string_view>);
static_assert(serde::detail::borrowed_string_sink_c<std::string_view>);
static_assert(not serde::detail::string_sink_c<char const*>);
static_assert(not serde::detail::borrowed_string_sink_c<char const*>);

// A str_c type that copies the run it is built from is still accepted. It rides
// the borrowed path, which is safe for a type that copies, rather than being
// refused for lacking push_back.
struct CopyingString
{
  std::string held;

  explicit CopyingString(std::string_view s) : held{s}
  {}
  operator std::string_view() const
  {
    return held;
  }
};
static_assert(str_c<CopyingString>);
static_assert(not serde::detail::string_sink_c<CopyingString>);
static_assert(serde::detail::borrowed_string_sink_c<CopyingString>);

// A sink is never also a borrowed destination, so no string type can reach both
// readers.
static_assert(not serde::detail::borrowed_string_sink_c<std::string>);
static_assert(not serde::detail::borrowed_string_sink_c<std::array<char, 5>>);
static_assert(not serde::detail::borrowed_string_sink_c<heapless::string<8>>);

// The borrowed read exists only where there is a buffer to point at. On the
// streaming cursor the input arrives a character at a time, so the same member
// is a compile error:
//
//   json::deserializer{std::istringstream{...}}.load<std::string_view>();
//
//   error: static assertion failed: std::basic_string_view<char> cannot be a
//   JSON string destination on this cursor: a borrowed read needs a contiguous
//   input to point at, and this deserializer reads a character at a time (use
//   std::string, or deserialize from a contiguous input)
static_assert(json::deserializer<std::string_view::const_iterator>::bulk_scan);
static_assert(not json::deserializer<std::istreambuf_iterator<char>>::bulk_scan);

struct Borrowed
{
  std::string_view text;
  int              n;
};

TEST_CASE("reflex::serde::json: a string destination that owns its storage")
{
  const std::string_view in =
      JSON({"small":"abc","chars":"Hello","owned":"Hello, world!"});

  SUBCASE("round-trips through a contiguous cursor")
  {
    const auto value = load_contiguous<StringSinks>(in);
    CHECK_EQ(std::string_view{value.small}, "abc"sv);
    CHECK_EQ(value.chars, std::array<char, 5>{'H', 'e', 'l', 'l', 'o'});
    CHECK_EQ(value.owned, "Hello, world!");
  }

  SUBCASE("round-trips through a streaming cursor")
  {
    CHECK_EQ(load_streaming<StringSinks>(in), load_contiguous<StringSinks>(in));
  }

  SUBCASE("and back out again")
  {
    CHECK_EQ(json_dump(load_contiguous<StringSinks>(in)), in);
  }

  SUBCASE("a non-owning member still serializes")
  {
    CHECK_EQ(json_dump(Borrowed{"abc", 1}), JSON({"text":"abc","n":1}));
  }
}

TEST_CASE("reflex::serde::json: a borrowed string destination")
{
  // Held by name, not as a temporary: the views read out of it point into it.
  const std::string in = JSON({"text":"hello there","n":7});

  SUBCASE("points into the input rather than copying it")
  {
    const auto v = json::deserializer{std::string_view{in}}.load<Borrowed>();
    CHECK_EQ(v.text, "hello there"sv);
    CHECK_EQ(v.n, 7);

    // The point of the whole exercise. Comparing content would pass for a copy
    // too, so the check is on the address: the member must alias the input.
    const char* const lo = in.data();
    const char* const hi = in.data() + in.size();
    CHECK(v.text.data() >= lo);
    CHECK(v.text.data() + v.text.size() <= hi);
    CHECK_EQ(v.text.data(), in.data() + in.find("hello there"));
  }

  SUBCASE("borrows from an owning string input too")
  {
    const auto v = json::deserializer{in}.load<Borrowed>();
    CHECK_EQ(v.text.data(), in.data() + in.find("hello there"));
  }

  SUBCASE("an escaped value throws rather than copying or dangling")
  {
    const std::string esc = JSON({"text":"a\nb","n":7});
    CHECK_THROWS_AS(
        (json::deserializer{std::string_view{esc}}.load<Borrowed>()), std::runtime_error);
  }

  SUBCASE("an escape anywhere in the value is enough")
  {
    // A doubled quote, a doubled backslash, an escape in the middle, a unicode
    // escape and one at the very end. None of these is a run of the input.
    const std::array<std::string_view, 5> bodies{
        "\\\"", "\\\\", "x\\ty", "\\u0041", "tail\\n"};
    for(const std::string_view body : bodies)
    {
      const std::string doc = "{\"text\":\"" + std::string{body} + "\",\"n\":7}";
      CHECK_THROWS_AS(
          (json::deserializer{std::string_view{doc}}.load<Borrowed>()), std::runtime_error);
    }
  }

  SUBCASE("an empty value borrows nothing and is fine")
  {
    const std::string empty = JSON({"text":"","n":7});
    const auto        v     = json::deserializer{std::string_view{empty}}.load<Borrowed>();
    CHECK(v.text.empty());
    CHECK_EQ(v.n, 7);
  }

  SUBCASE("an unterminated value still throws")
  {
    CHECK_THROWS((json::deserializer{R"({"text":"abc)"sv}.load<Borrowed>()));
  }

  SUBCASE("a bare view, no aggregate around it")
  {
    const std::string doc = R"("just a string")";
    const auto        v   = json::deserializer{std::string_view{doc}}.load<std::string_view>();
    CHECK_EQ(v, "just a string"sv);
    CHECK_EQ(v.data(), doc.data() + 1);
  }
}

// A map used to serialize without reading back, for want of an object_visitor
// specialization: the object-reading overload is constrained on
// object_visitable_c, the object-writing one is not. Write-only, and nothing in
// the type system said so. The gap was in the shared visitor, so it was the
// same in xml and csv.
struct[[= derive(Debug)]] Config
{
  std::string                name;
  std::map<std::string, int> limits;

  constexpr bool operator==(Config const&) const = default;
};

TEST_CASE("reflex::serde::json: a map round-trips")
{
  SUBCASE("a bare map")
  {
    const auto m = std::map<std::string, int>{
        {"a", 1},
        {"b", 2}
    };
    const auto encoded = json_dump(m);
    CHECK_EQ(encoded, JSON({"a":1,"b":2}));
    CHECK_EQ(json::deserializer{encoded}.load<std::map<std::string, int>>(), m);
  }

  SUBCASE("a map inside an aggregate")
  {
    const Config expected{
        "x", {{"soft", 1}, {"hard", 2}}
    };
    const auto encoded = json_dump(expected);
    CHECK_EQ(encoded, JSON({"name":"x","limits":{"hard":2,"soft":1}}));
    CHECK_EQ(json::deserializer{encoded}.load<Config>(), expected);
  }

  SUBCASE("a map of aggregates")
  {
    const auto nested = std::map<std::string, Config>{
        {"k", {"x", {{"soft", 1}}}}
    };
    const auto encoded = json_dump(nested);
    CHECK_EQ(encoded, JSON({"k":{"name":"x","limits":{"soft":1}}}));
    CHECK_EQ(json::deserializer{encoded}.load<std::map<std::string, Config>>(), nested);
  }

  // A key the document names and the map does not hold is created, which is the
  // whole difference from the aggregate visitor: an aggregate cannot grow a
  // member, so a miss there can only be an error.
  SUBCASE("an empty map is filled from the document")
  {
    const std::string_view in = JSON({"a":1});
    const auto             m  = json::deserializer{in}.load<std::map<std::string, int>>();
    CHECK_EQ(m.size(), 1);
    CHECK_EQ(m.at("a"), 1);
  }
}

// Every test in this file spells its input "..."s or "..."sv, so the one thing a
// caller reaches for first went untried: a bare literal. It used to reach the
// stream constructor, whose end iterator is default-constructed and therefore
// null, and the parse aborted inside ranges::advance on the first skip.
TEST_CASE("reflex::serde::json::deserializer: a string literal is an input")
{
  SUBCASE("a scalar")
  {
    CHECK_EQ(json::deserializer{"42"}.load<int>(), 42);
  }

  SUBCASE("the trailing NUL is not part of the document")
  {
    json::deserializer d{"42"};
    CHECK_EQ(d.input().size(), 2);
    CHECK_EQ(d.input(), "42"sv);
  }

  SUBCASE("an aggregate")
  {
    const auto value = json::deserializer{R"({"a":1,"b":2})"}.load<std::map<std::string, int>>();
    CHECK_EQ(value.at("a"), 1);
    CHECK_EQ(value.at("b"), 2);
  }

  // A char buffer that was never NUL-terminated is read to its bound rather
  // than past it. Only one trailing NUL is dropped, so a document ending in a
  // deliberate NUL keeps the rest.
  SUBCASE("an array with no terminator")
  {
    const char raw[] = {'4', '2'};
    CHECK_EQ(json::deserializer{raw}.input().size(), 2);
    CHECK_EQ(json::deserializer{raw}.load<int>(), 42);
  }
}

// An output iterator that carries its position by value. write_char advances it
// through the postfix increment, so write_raw must assign back the iterator
// ranges::copy returns or the bulk write is overwritten by the next byte.
namespace
{
  struct cursor_out
  {
    using iterator_category = std::output_iterator_tag;
    using value_type        = void;
    using difference_type   = std::ptrdiff_t;
    using pointer           = void;
    using reference         = void;

    char* p{};

    cursor_out& operator*() { return *this; }
    cursor_out& operator=(char c)
    {
      *p = c;
      return *this;
    }
    cursor_out& operator++() { ++p; return *this; }
    cursor_out  operator++(int)
    {
      auto copy = *this;
      ++p;
      return copy;
    }
  };
} // namespace

TEST_CASE("reflex::serde::json: serializing through a position-carrying iterator")
{
  char buffer[32] = {};

  reflex::serde::json::serializer<cursor_out> ser{cursor_out{buffer}};
  reflex::serde::serialize(ser, std::string_view{"ab"});

  CHECK_EQ(std::string_view{buffer}, "\"ab\"");
}
