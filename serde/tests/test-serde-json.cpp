#include <doctest/doctest.h>

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
    serde::json::deserializer<InputIt>& de,
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
    serde::json::deserializer<InputIt>& de,
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

  // A rename containing a dot serializes correctly but cannot be read back:
  // object_visit treats a key as a dotted path and splits on '.', so no member
  // matches either half. Pre-existing, pinned here so the asymmetry is not
  // rediscovered as a regression.
  struct[[= derive(Debug)]] DottedRename
  {
    [[= serde::rename{"outer.inner"}]] int value;
    constexpr bool operator==(DottedRename const&) const = default;
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

  SUBCASE("infinities and NaN are still emitted as invalid JSON")
  {
    // Pre-existing defect, pinned. inf and nan are not JSON. Fixing it is a
    // wire-format decision and is filed separately, not made here.
    CHECK_EQ(json_dump(std::numeric_limits<double>::infinity()), "inf");
    CHECK_EQ(json_dump(-std::numeric_limits<double>::infinity()), "-inf");
    CHECK_EQ(json_dump(std::numeric_limits<double>::quiet_NaN()), "nan");
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

  SUBCASE("a rename containing a dot writes but does not read back")
  {
    const auto encoded = json_dump(DottedRename{42});
    CHECK_EQ(encoded, "{\"outer.inner\":42}");
    CHECK_THROWS(json::deserializer{encoded}.load<DottedRename>());
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
