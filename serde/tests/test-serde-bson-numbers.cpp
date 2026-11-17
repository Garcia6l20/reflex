#include <doctest/doctest.h>

import reflex.serde.bson;
import reflex.serde.json;
import reflex.serde.toml;
import reflex.serde.yaml;

import std;

using namespace reflex;
using namespace reflex::serde;

namespace reflex::serde::json
{
template <typename OutputIt, auto Opts>
OutputIt
    tag_invoke(tag_t<serde::serialize>, serializer<OutputIt, Opts>& ser, bson::datetime const& dt)
{
  return tag_invoke(tag_t<serde::serialize>{}, ser, std::format("{}", dt));
}

template <typename It, auto Opts>
bson::datetime tag_invoke(
    tag_t<serde::deserialize>,
    deserializer<It, Opts>& de,
    std::type_identity<bson::datetime>)
{
  auto s = tag_invoke(tag_t<serde::deserialize>{}, de, std::type_identity<std::string>{});
  return reflex::parse_or_throw<bson::datetime>(s);
}
}

namespace reflex::serde::yaml
{
template <typename OutputIt, auto Opts>
OutputIt
    tag_invoke(tag_t<serde::serialize>, serializer<OutputIt, Opts>& ser, bson::datetime const& dt)
{
  return tag_invoke(tag_t<serde::serialize>{}, ser, std::format("{}", dt));
}

template <typename It, auto Opts>
bson::datetime tag_invoke(
    tag_t<serde::deserialize>,
    deserializer<It, Opts>& de,
    std::type_identity<bson::datetime>)
{
  auto s = tag_invoke(tag_t<serde::deserialize>{}, de, std::type_identity<std::string>{});
  return reflex::parse_or_throw<bson::datetime>(s);
}
}

namespace reflex::serde::toml
{
template <typename OutputIt, auto Opts>
OutputIt
    tag_invoke(tag_t<serde::serialize>, serializer<OutputIt, Opts>& ser, bson::datetime const& dt)
{
  return tag_invoke(tag_t<serde::serialize>{}, ser, std::format("{}", dt));
}

template <typename It, auto Opts>
bson::datetime tag_invoke(
    tag_t<serde::deserialize>,
    deserializer<It, Opts>& de,
    std::type_identity<bson::datetime>)
{
  auto s = tag_invoke(tag_t<serde::deserialize>{}, de, std::type_identity<std::string>{});
  return reflex::parse_or_throw<bson::datetime>(s);
}
}

namespace
{
using bytes            = std::vector<std::byte>;
using numbers_user_var = poly::var<int, double, std::string>;

bson::value numbers_from_json(std::string const& text)
{
  return json::deserializer{text}.load<bson::value>();
}

bson::value numbers_from_json_stream(std::string const& text)
{
  std::istringstream in{text};
  return json::deserializer{in}.load<bson::value>();
}

bson::value numbers_from_yaml(std::string const& text)
{
  return yaml::deserializer{text}.load<bson::value>();
}

bson::value numbers_from_toml(std::string_view token)
{
  auto doc = toml::deserializer{std::format("x = {}\n", token)}.load<bson::value>();
  return doc["x"];
}

template <typename T> void numbers_check_is(std::string_view token, bool toml = true)
{
  CAPTURE(token);
  CHECK(numbers_from_json(std::string{token}).is<T>());
  CHECK(numbers_from_json_stream(std::string{token}).is<T>());
  CHECK(numbers_from_yaml(std::string{token}).is<T>());
  if(toml)
  {
    CHECK(numbers_from_toml(token).is<T>());
  }
}

bson::value numbers_mixed()
{
  return bson::value{
      {"i", std::int32_t{7}                   },
      {"l", std::int64_t{1} << 40             },
      {"d", 2.5                               },
      {"a", bson::array{std::int32_t{-3}, 1.5}}
  };
}

std::string numbers_json_of(bson::value const& value)
{
  std::string      out;
  json::serializer ser{out};
  ser.dump(value);
  return out;
}

std::string numbers_yaml_of(bson::value const& value)
{
  std::string      out;
  yaml::serializer ser{out};
  ser.dump(value);
  return out;
}

std::string numbers_toml_of(bson::value const& value)
{
  std::string      out;
  toml::serializer ser{out};
  ser.dump(value);
  return out;
}

bytes numbers_bson_of(bson::value const& value)
{
  bytes            out;
  bson::serializer ser{out};
  serde::serialize(ser, value);
  return out;
}

void numbers_check_round_trip(bson::value back)
{
  const auto original = numbers_mixed();
  CHECK(back == original);
  CHECK(back["i"].is<std::int32_t>());
  CHECK(back["l"].is<std::int64_t>());
  CHECK(back["d"].is<double>());
  CHECK(back["a"][0].is<std::int32_t>());
  CHECK(back["a"][1].is<double>());
  CHECK(numbers_bson_of(back) == numbers_bson_of(original));
}
}

TEST_CASE("bson::value from text: integers within int32 read as int32")
{
  for(auto token : {"0", "7", "-7", "2147483647", "-2147483648"})
  {
    numbers_check_is<std::int32_t>(token);
  }
  CHECK(numbers_from_json("2147483647").as<std::int32_t>() == 2147483647);
  CHECK(numbers_from_yaml("-2147483648").as<std::int32_t>() == -2147483648);
  CHECK(numbers_from_toml("-7").as<std::int32_t>() == -7);
}

TEST_CASE("bson::value from text: integers past int32 read as int64")
{
  for(auto token :
      {"2147483648", "-2147483649", "5000000000", "9223372036854775807", "-9223372036854775808"})
  {
    numbers_check_is<std::int64_t>(token);
  }
  CHECK(numbers_from_json("5000000000").as<std::int64_t>() == 5000000000);
  CHECK(
      numbers_from_json_stream("-9223372036854775808").as<std::int64_t>()
      == std::numeric_limits<std::int64_t>::min());
  CHECK(
      numbers_from_yaml("9223372036854775807").as<std::int64_t>()
      == std::numeric_limits<std::int64_t>::max());
  CHECK(
      numbers_from_toml("-9223372036854775808").as<std::int64_t>()
      == std::numeric_limits<std::int64_t>::min());
}

TEST_CASE("bson::value from text: non-integers read as double")
{
  for(auto token : {"1.0", "1e3", "-0.0", "2.5"})
  {
    numbers_check_is<double>(token);
  }
  CHECK(numbers_from_json("1e3").as<double>() == 1000.0);
  CHECK(std::signbit(numbers_from_yaml("-0.0").as<double>()));
}

TEST_CASE("bson::value from text: integers past int64")
{
  for(auto token : {"9223372036854775808", "-9223372036854775809"})
  {
    numbers_check_is<double>(token, false);
  }
  CHECK(numbers_from_json("9223372036854775808").as<double>() == 9223372036854775808.0);
  CHECK_THROWS_WITH(
      numbers_from_toml("9223372036854775808"), "TOML: '9223372036854775808' is not an integer");
  CHECK_THROWS_WITH(
      numbers_from_toml("-9223372036854775809"), "TOML: '-9223372036854775809' is not an integer");
}

TEST_CASE("bson::value from YAML: prefixed integers and non-finite numbers")
{
  const auto hex = numbers_from_yaml("0x10");
  REQUIRE(hex.is<std::int32_t>());
  CHECK(hex.as<std::int32_t>() == 16);

  const auto oct = numbers_from_yaml("0o17");
  REQUIRE(oct.is<std::int32_t>());
  CHECK(oct.as<std::int32_t>() == 15);

  const auto big = numbers_from_yaml("0x100000000");
  REQUIRE(big.is<std::int64_t>());
  CHECK(big.as<std::int64_t>() == 0x100000000);

  CHECK_THROWS_WITH(
      numbers_from_yaml("0x8000000000000000"), "YAML: '0x8000000000000000' is not an integer");
  CHECK_THROWS_WITH(
      numbers_from_yaml("0o1000000000000000000000"),
      "YAML: '0o1000000000000000000000' is not an integer");

  const auto inf = numbers_from_yaml(".inf");
  REQUIRE(inf.is<double>());
  CHECK(std::isinf(inf.as<double>()));
}

TEST_CASE("bson::value non-finite doubles round trip through YAML and TOML in their native form")
{
  constexpr auto nan = std::numeric_limits<double>::quiet_NaN();
  constexpr auto inf = std::numeric_limits<double>::infinity();

  const bson::value original{
      {"n", nan                        },
      {"p", inf                        },
      {"m", -inf                       },
      {"a", bson::array{nan, -inf, inf}}
  };

  const auto check_back = [inf](bson::value back) {
    REQUIRE(back["n"].is<double>());
    CHECK(std::isnan(back["n"].as<double>()));
    REQUIRE(back["p"].is<double>());
    CHECK(back["p"].as<double>() == inf);
    REQUIRE(back["m"].is<double>());
    CHECK(back["m"].as<double>() == -inf);
    REQUIRE(back["a"][0].is<double>());
    CHECK(std::isnan(back["a"][0].as<double>()));
    REQUIRE(back["a"][1].is<double>());
    CHECK(back["a"][1].as<double>() == -inf);
    REQUIRE(back["a"][2].is<double>());
    CHECK(back["a"][2].as<double>() == inf);
  };

  SUBCASE("YAML")
  {
    const auto text = numbers_yaml_of(original);
    CHECK_EQ(text, "a:\n  - .nan\n  - -.inf\n  - .inf\nm: -.inf\n'n': .nan\np: .inf");
    const auto back = numbers_from_yaml(text);
    check_back(back);
    CHECK_EQ(numbers_yaml_of(back), text);
  }
  SUBCASE("TOML")
  {
    const auto text = numbers_toml_of(original);
    CHECK_EQ(text, "a = [nan, -inf, inf]\nm = -inf\nn = nan\np = inf");
    const auto back = toml::deserializer{text}.load<bson::value>();
    check_back(back);
    CHECK_EQ(numbers_toml_of(back), text);
  }
}

namespace
{
struct numbers_decimal_case
{
  bson::decimal128 value;
  std::string      text;
  std::string      yaml;
};

std::vector<numbers_decimal_case> numbers_decimal_cases()
{
  constexpr auto inf = std::numeric_limits<bson::decimal128>::infinity();
  constexpr auto nan = std::numeric_limits<bson::decimal128>::quiet_NaN();
  return {
      {bson::decimal128{0},                     "0",                                 "'0'"      },
      {-bson::decimal128{0},                    "-0",                                "'-0'"     },
      {bson::decimal128{1.5},                   "1.5",                               "'1.5'"    },
      {1e100f128,                               "1e+100",                            "'1e+100'" },
      {123456789012345678901234567890123.0f128, "123456789012345678901234567890123",
       "'123456789012345678901234567890123'"                                                    },
      {nan,                                     "NaN",                               "NaN"      },
      {inf,                                     "Infinity",                          "Infinity" },
      {-inf,                                    "-Infinity",                         "-Infinity"},
  };
}

void numbers_check_decimal(bson::value const& lifted, bson::decimal128 original)
{
  REQUIRE(lifted.is<bson::decimal128>());
  const auto back = lifted.as<bson::decimal128>();
  if(std::isnan(original))
  {
    CHECK(std::isnan(back));
  }
  else
  {
    CHECK(back == original);
    CHECK(std::signbit(back) == std::signbit(original));
  }
}

void numbers_check_decimal_relay(bson::value const& read, bson::decimal128 original)
{
  const auto lifted = numbers_from_json(numbers_json_of(read));
  numbers_check_decimal(lifted.as_object().at("x"), original);
  numbers_check_decimal(lifted.as_object().at("a").as_array().at(0), original);
}

std::string const& numbers_decimal_text(bson::value const& read)
{
  auto const& wrapper = read.as_object().at("x").as_object().at("$numberDecimal");
  REQUIRE(wrapper.is<std::string>());
  return wrapper.as<std::string>();
}
}

bson::value numbers_decimal_doc(bson::decimal128 number)
{
  return bson::value{
      {"x", number             },
      {"a", bson::array{number}}
  };
}

TEST_CASE("bson::value decimal128 writes as $numberDecimal in YAML")
{
  for(auto const& [number, text, yaml_text] : numbers_decimal_cases())
  {
    CAPTURE(text);
    const auto written = numbers_yaml_of(numbers_decimal_doc(number));
    CHECK_EQ(
        written, std::format("a:\n  - $numberDecimal: {0}\nx:\n  $numberDecimal: {0}", yaml_text));
    CHECK_EQ(numbers_yaml_of(bson::value{number}), std::format("$numberDecimal: {}", yaml_text));
    const auto back = numbers_from_yaml(written);
    numbers_check_decimal(back.as_object().at("x"), number);
    numbers_check_decimal(back.as_object().at("a").as_array().at(0), number);
    numbers_check_decimal(numbers_from_yaml(std::format("$numberDecimal: {}", yaml_text)), number);
    CHECK_EQ(numbers_yaml_of(back), written);
  }
}

TEST_CASE("bson::value decimal128 writes as $numberDecimal in TOML")
{
  for(auto const& [number, text, yaml_text] : numbers_decimal_cases())
  {
    CAPTURE(text);
    const auto written = numbers_toml_of(numbers_decimal_doc(number));
    CHECK_EQ(
        written, std::format(
                     "a = [{{ \"$numberDecimal\" = \"{0}\" }}]\nx = {{ \"$numberDecimal\" = "
                     "\"{0}\" }}",
                     text));
    CHECK_EQ(
        numbers_toml_of(bson::value{bson::object{{"x", number}}}),
        std::format("x = {{ \"$numberDecimal\" = \"{}\" }}", text));
    const auto back = toml::deserializer{written}.load<bson::value>();
    CHECK_EQ(numbers_decimal_text(back), text);
    numbers_check_decimal_relay(back, number);
  }
}

struct numbers_commented
{
  bson::value d;
  bson::value o;
  int         n = 0;
};

struct numbers_plain_float128
{
  std::float128_t                a = 0;
  std::vector<std::float128_t>   v;
  std::optional<std::float128_t> o;
  std::optional<std::float128_t> e;
};

TEST_CASE("bson::value decimal128 writes as $numberDecimal in YAML comments mode")
{
  constexpr yaml::options with_comments{.comments = true};

  serde::comments_of<numbers_commented> c;
  c.d.trailing = "decimal";
  c.o.leading  = {"object"};
  c.n.trailing = "plain";

  const numbers_commented value{
      .d = bson::decimal128{1.5},
      .o = bson::object{{"x", bson::decimal128{-2}}},
      .n = 3,
  };
  std::string out;
  yaml::serializer{out, std::cw<with_comments>}.dump(value, c);
  CHECK_EQ(
      out, "d: # decimal\n"
           "  $numberDecimal: '1.5'\n"
           "# object\n"
           "o:\n"
           "  x:\n"
           "    $numberDecimal: '-2'\n"
           "'n': 3 # plain\n");
}

TEST_CASE("a plain float128 keeps its plain form in YAML and TOML")
{
  const numbers_plain_float128 value{
      .a = 1.5f128,
      .v = {-0.0f128, 1e100f128},
      .o = 2.5f128,
      .e = std::nullopt,
  };

  std::string      yaml_out;
  yaml::serializer yaml_ser{yaml_out};
  yaml_ser.dump(value);
  CHECK_EQ(yaml_out, "a: 1.5\nv:\n  - -0\n  - 1e+100\no: 2.5\ne: null");

  std::string      toml_out;
  toml::serializer toml_ser{toml_out};
  toml_ser.dump(value);
  CHECK_EQ(toml_out, "a = 1.5\nv = [-0.0, 1e+100]\no = 2.5");
}

TEST_CASE("bson::value from JSON: numbers inside containers")
{
  for(auto read : {numbers_from_json, numbers_from_json_stream})
  {
    auto v = read(R"({"a": [2147483648, -1, 0.5], "b": 3 })");
    CHECK(v["a"][0].is<std::int64_t>());
    CHECK(v["a"][1].is<std::int32_t>());
    CHECK(v["a"][2].is<double>());
    CHECK(v["b"].is<std::int32_t>());
    CHECK_THROWS_WITH(read("[1x]"), "Failed to parse number");
    CHECK_THROWS_WITH(read("[1.2.3]"), "Failed to parse number");
  }
}

TEST_CASE("bson::value text round trip keeps integer widths")
{
  const auto original = numbers_mixed();

  SUBCASE("JSON")
  {
    const auto text = numbers_json_of(original);
    numbers_check_round_trip(numbers_from_json(text));
    numbers_check_round_trip(numbers_from_json_stream(text));
    CHECK(numbers_json_of(numbers_from_json(text)) == text);
  }
  SUBCASE("YAML")
  {
    const auto text = numbers_yaml_of(original);
    numbers_check_round_trip(numbers_from_yaml(text));
    CHECK(numbers_yaml_of(numbers_from_yaml(text)) == text);
  }
  SUBCASE("TOML")
  {
    const auto text = numbers_toml_of(original);
    const auto back = toml::deserializer{text}.load<bson::value>();
    numbers_check_round_trip(back);
    CHECK(numbers_toml_of(back) == text);
  }
}

TEST_CASE("other dynamic values keep reading integers as before")
{
  CHECK(json::deserializer{std::string{"7"}}.load<json::value>().is<json::number>());
  CHECK(yaml::deserializer{std::string{"7"}}.load<yaml::value>().is<yaml::number>());
  CHECK(json::deserializer{std::string{"7"}}.load<numbers_user_var>().is<double>());
  static_assert(not serde::detail::ext_json_value<json::value>::enabled);
  static_assert(not serde::detail::ext_json_value<yaml::value>::enabled);
  static_assert(not serde::detail::ext_json_value<toml::value>::enabled);
  static_assert(not serde::detail::ext_json_value<numbers_user_var>::enabled);
  static_assert(serde::detail::ext_json_value<bson::value>::enabled);
}
