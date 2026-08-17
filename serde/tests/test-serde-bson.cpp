#include <doctest/doctest.h>

import reflex.serde.bson;

import std;

using namespace reflex;
using namespace reflex::serde;
using namespace std::literals;

struct[[= serde::naming::camel_case]] S
{
  int                                    int_member;
  std::string                            string_member;
  [[= serde::naming::kebab_case]] double double_member;

  bool operator==(S const&) const = default;
};

enum class Color
{
  Red,
  Green,
  Blue
};

struct Optionals
{
  std::optional<int>         number;
  std::optional<std::string> text;

  bool operator==(Optionals const&) const = default;
};

TEST_CASE("reflex::serde::bson: base types round-trip")
{
  using bson::null;

  SUBCASE("null")
  {
    std::vector<std::byte> out;
    bson::serializer       ser{out};
    ser.dump(null);
    auto value = bson::deserializer{out}.load<bson::null_t>();
    CHECK(value == null);
  }

  SUBCASE("string")
  {
    std::vector<std::byte> out;
    bson::serializer       ser{out};
    ser.dump(std::string{"Hello, world!"});
    auto value = bson::deserializer{out}.load<std::string>();
    CHECK_EQ(value, "Hello, world!");
  }

  SUBCASE("int32")
  {
    std::vector<std::byte> out;
    bson::serializer       ser{out};
    ser.dump(42);
    auto value = bson::deserializer{out}.load<int>();
    CHECK_EQ(value, 42);
  }

  SUBCASE("int64")
  {
    std::vector<std::byte> out;
    bson::serializer       ser{out};
    ser.dump(1ll << 40);
    auto value = bson::deserializer{out}.load<long long>();
    CHECK_EQ(value, (1ll << 40));
  }

  SUBCASE("double")
  {
    std::vector<std::byte> out;
    bson::serializer       ser{out};
    ser.dump(3.14);
    auto value = bson::deserializer{out}.load<double>();
    CHECK(value == doctest::Approx(3.14));
  }

  SUBCASE("boolean")
  {
    std::vector<std::byte> out;
    bson::serializer       ser{out};
    ser.dump(true);
    auto value = bson::deserializer{out}.load<bool>();
    CHECK(value);
  }

  SUBCASE("enum")
  {
    std::vector<std::byte> out;
    bson::serializer       ser{out};
    ser.dump(Color::Green);
    auto value = bson::deserializer{out}.load<Color>();
    CHECK(value == Color::Green);
  }
}

TEST_CASE("reflex::serde::bson: sequence and map")
{
  SUBCASE("sequence")
  {
    std::vector<std::byte> out;
    bson::serializer       ser{out};
    std::vector<int>       arr = {1, 2, 3};

    ser.dump(arr);
    auto value = bson::deserializer{out}.load<std::vector<int>>();

    CHECK_EQ(value.size(), 3);
    CHECK_EQ(value[0], 1);
    CHECK_EQ(value[1], 2);
    CHECK_EQ(value[2], 3);
  }

  SUBCASE("map")
  {
    std::vector<std::byte> out;
    bson::serializer       ser{out};
    auto                   obj = std::map<std::string, int>{
        {"a", 1},
        {"b", 2}
    };

    ser.dump(obj);
    auto value = bson::deserializer{out}.load<std::map<std::string, int>>();

    CHECK_EQ(value.size(), 2);
    CHECK_EQ(value.at("a"), 1);
    CHECK_EQ(value.at("b"), 2);
  }
}

TEST_CASE("reflex::serde::bson: aggregate")
{
  std::vector<std::byte> out;
  bson::serializer       ser{out};

  S s{42, "Hello, world!", 3.14};
  ser.dump(s);
  auto value = bson::deserializer{out}.load<S>();

  CHECK_EQ(value.int_member, 42);
  CHECK_EQ(value.string_member, "Hello, world!");
  CHECK(value.double_member == doctest::Approx(3.14));
}

TEST_CASE("reflex::serde::bson: explicit bson scalar types")
{
  SUBCASE("bson::int32")
  {
    std::vector<std::byte> out;
    bson::serializer       ser{out};
    auto                   input = bson::int32{42};
    ser.dump(input);
    auto value = bson::deserializer{out}.load<bson::int32>();
    CHECK_EQ(value, input);
  }

  SUBCASE("bson::int64")
  {
    std::vector<std::byte> out;
    bson::serializer       ser{out};
    auto                   input = bson::int64{1ll << 40};
    ser.dump(input);
    auto value = bson::deserializer{out}.load<bson::int64>();
    CHECK_EQ(value, input);
  }

  SUBCASE("bson::decimal128")
  {
    std::vector<std::byte> out;
    bson::serializer       ser{out};
    bson::decimal128       input{42.2};
    ser.dump(input);
    auto value = bson::deserializer{out}.load<bson::decimal128>();
    CHECK_EQ(value, input);
  }

  SUBCASE("bson::datetime")
  {
    std::vector<std::byte> out;
    bson::serializer       ser{out};
    bson::datetime         input{1'700'000'000'123ll};
    ser.dump(input);
    auto value = bson::deserializer{out}.load<bson::datetime>();
    CHECK_EQ(value, input);
  }
}

TEST_CASE("reflex::serde::bson: value from map")
{
  std::vector<std::byte> out;
  bson::serializer       ser{out};

  auto obj = std::map<std::string, int>{
      {"a", 1},
      {"b", 2}
  };
  std::println("Input: {}", obj);
  ser.dump(obj);
  std::println("Serialized: {}", out);

  auto value = bson::deserializer{out}.load();
  std::println("Deserialized: {}", value);

  CHECK(value.is_object());
  CHECK_EQ(value.as<bson::object>().size(), 2);
  CHECK_EQ(value.as<bson::object>().at("a"), 1);
  CHECK_EQ(value.as<bson::object>().at("b"), 2);
}

TEST_CASE("reflex::serde::bson: value preserves bson scalar types")
{
  std::vector<std::byte> out;
  bson::serializer       ser{out};

  auto typed = std::map<std::string, bson::value>{
      {"i32",  bson::int32{7}                     },
      {"i64",  bson::int64{1ll << 40}             },
      {"d128", bson::decimal128{42.2}             },
      {"dt",   bson::datetime{1'701'234'567'890ll}},
  };

  ser.dump(typed);
  auto value = bson::deserializer{out}.load();

  CHECK(value.is_object());
  auto const& obj = value.as<bson::object>();

  CHECK(obj.at("i32").is<bson::int32>());
  CHECK_EQ(obj.at("i32").as<bson::int32>(), 7);

  CHECK(obj.at("i64").is<bson::int64>());
  CHECK_EQ(obj.at("i64").as<bson::int64>(), (1ll << 40));

  CHECK(obj.at("d128").is<bson::decimal128>());
  CHECK_EQ(obj.at("d128").as<bson::decimal128>(), typed.at("d128").as<bson::decimal128>());

  CHECK(obj.at("dt").is<bson::datetime>());
  CHECK_EQ(bson::datetime{1'701'234'567'890ll}, obj.at("dt").as<bson::datetime>());
}

TEST_CASE("reflex::serde::bson: malformed input throws")
{
  SUBCASE("empty input")
  {
    std::vector<std::byte> input;
    CHECK_THROWS_AS(
        (bson::deserializer{input.begin(), input.end()}.load<int>()), std::runtime_error);
  }

  SUBCASE("truncated document")
  {
    std::vector<std::byte> out;
    bson::serializer       ser{out};
    ser.dump(
        std::map<std::string, int>{
            {"a", 1}
    });
    out.pop_back();
    CHECK_THROWS_AS(
        (bson::deserializer{out}.load<std::map<std::string, int>>()), std::runtime_error);
  }

  SUBCASE("invalid type tag")
  {
    std::vector<std::byte> out;
    bson::serializer       ser{out};
    ser.dump(123);
    out[4] = std::byte{0x7F};
    CHECK_THROWS_AS((bson::deserializer{out}.load<int>()), std::runtime_error);
  }
}

namespace
{
struct inner_t
{
  int x;

  bool operator==(inner_t const&) const = default;
};

struct outer_t
{
  inner_t     a;
  std::string b;

  bool operator==(outer_t const&) const = default;
};

void put_i32(std::vector<std::byte>& b, std::size_t at, std::int32_t v)
{
  const auto raw = static_cast<std::uint32_t>(v);
  for(std::size_t i = 0; i < 4; ++i)
  {
    b[at + i] = static_cast<std::byte>((raw >> (8 * i)) & 0xFFu);
  }
}

template <typename T> std::vector<std::byte> encode(T const& value)
{
  std::vector<std::byte> out;
  bson::serializer       ser{out};
  ser.dump(value);
  return out;
}
} // namespace

// A serializer handed the byte container encodes straight into it. A serializer handed a bare
// output iterator cannot, and goes through a temporary. Both must produce the same bytes.
TEST_CASE("reflex::serde::bson: every sink kind produces the same bytes")
{
  const S sample{42, "Hello, world!", 3.14};

  std::vector<std::byte> via_container;
  {
    bson::serializer ser{via_container};
    ser.dump(sample);
  }

  std::vector<std::byte> via_iterator;
  {
    bson::serializer<std::back_insert_iterator<std::vector<std::byte>>> ser{
        std::back_inserter(via_iterator)};
    ser.dump(sample);
  }

  std::ostringstream via_stream_buf;
  {
    bson::serializer ser{via_stream_buf};
    ser.dump(sample);
  }
  const auto stream_str = via_stream_buf.str();

  CHECK_EQ(via_container, via_iterator);
  REQUIRE_EQ(stream_str.size(), via_container.size());
  CHECK(std::memcmp(stream_str.data(), via_container.data(), via_container.size()) == 0);

  // And all three still read back.
  CHECK_EQ(bson::deserializer{via_container}.load<S>(), sample);
  CHECK_EQ(bson::deserializer{via_iterator}.load<S>(), sample);
}

// read_document hands the callback a key borrowed from the input buffer. Nothing may
// keep it: the parsed result has to stay valid once the input is gone.
TEST_CASE("reflex::serde::bson: a parsed document outlives its input buffer")
{
  SUBCASE("map keys are copied out of the borrowed view")
  {
    const std::map<std::string, std::string> expected{
        {"alpha",                                        "one"  },
        {"a-much-longer-key-past-the-sso-limit-for-sure", "two"  },
        {"gamma",                                        "three"},
    };

    std::map<std::string, std::string> got;
    {
      const auto doc = encode(expected);
      got            = bson::deserializer{doc}.load<std::map<std::string, std::string>>();
    }
    CHECK_EQ(got, expected);
  }

  SUBCASE("nested bson::value object keys are copied")
  {
    bson::object nested;
    nested["deep-key-well-past-the-sso-limit-so-it-allocates"] = bson::value{std::string{"v"}};

    bson::object root;
    root["obj"] = bson::value{nested};

    bson::value got;
    {
      const auto doc = encode(root);
      got            = bson::deserializer{doc}.load<bson::value>();
    }

    REQUIRE(got.is<bson::object>());
    REQUIRE(got.as<bson::object>().contains("obj"));
    auto const& inner = got.as<bson::object>().at("obj");
    REQUIRE(inner.is<bson::object>());
    CHECK(inner.as<bson::object>().contains("deep-key-well-past-the-sso-limit-so-it-allocates"));
  }

  SUBCASE("aggregate members survive the input")
  {
    const S  expected{42, "Hello, world!", 3.14};
    S        got{};
    {
      const auto doc = encode(expected);
      got            = bson::deserializer{doc}.load<S>();
    }
    CHECK_EQ(got, expected);
  }
}

// A length read out of the document must be checked against what the input can actually supply.
// Before this, a declared length past the end of the buffer was copied out verbatim.
TEST_CASE("reflex::serde::bson: a length past the end of the input throws")
{
  const S sample{42, "Hello, world!", 3.14};

  SUBCASE("document length exceeds the buffer")
  {
    auto buf = encode(sample);
    put_i32(buf, 0, 0x0000FFFF);
    CHECK_THROWS_AS((bson::deserializer{buf}.load<S>()), std::runtime_error);
  }

  SUBCASE("string length exceeds the buffer")
  {
    auto buf = encode(sample);

    // Locate the string element's 4-byte length prefix through its unique payload.
    const std::string_view needle  = "Hello, world!";
    std::size_t            payload = 0;
    for(std::size_t i = 0; i + needle.size() <= buf.size(); ++i)
    {
      if(std::memcmp(buf.data() + i, needle.data(), needle.size()) == 0)
      {
        payload = i;
        break;
      }
    }
    REQUIRE(payload >= 4);

    auto huge = buf;
    put_i32(huge, payload - 4, 0x00100000);
    CHECK_THROWS_AS((bson::deserializer{huge}.load<S>()), std::runtime_error);

    // The tightest possible overrun, one byte more than the input holds.
    auto edge = buf;
    put_i32(edge, payload - 4, static_cast<std::int32_t>(buf.size() - payload + 1));
    CHECK_THROWS_AS((bson::deserializer{edge}.load<S>()), std::runtime_error);
  }

  SUBCASE("cstring with no terminator")
  {
    auto full = encode(sample);
    // Cut just inside the first key so it runs to the end of the buffer with no null byte.
    std::vector<std::byte> buf{full.begin(), full.begin() + 7};
    put_i32(buf, 0, static_cast<std::int32_t>(buf.size()));
    CHECK_THROWS_AS((bson::deserializer{buf}.load<S>()), std::runtime_error);
  }

  SUBCASE("nested document length exceeds its parent")
  {
    const auto full = encode(outer_t{{5}, std::string(64, 'p')});

    // The nested document follows the root length prefix, one type byte and the key "a\0".
    const std::size_t nested_at = 4 + 1 + 2;

    auto spans_buffer = full;
    put_i32(spans_buffer, nested_at, static_cast<std::int32_t>(full.size()));
    CHECK_THROWS_AS((bson::deserializer{spans_buffer}.load<outer_t>()), std::runtime_error);

    auto past_buffer = full;
    put_i32(past_buffer, nested_at, 0x00100000);
    CHECK_THROWS_AS((bson::deserializer{past_buffer}.load<outer_t>()), std::runtime_error);
  }
}

// The sweep that finds the length reads the targeted cases above miss. Every proper prefix of a
// valid document must throw, both as-is and with the root length corrected to the truncated size.
TEST_CASE("reflex::serde::bson: every truncation of a document throws")
{
  SUBCASE("flat, contiguous cursor")
  {
    const auto full = encode(S{42, "Hello, world!", 3.14});
    for(std::size_t n = 0; n < full.size(); ++n)
    {
      std::vector<std::byte> buf{full.begin(), full.begin() + static_cast<std::ptrdiff_t>(n)};
      CAPTURE(n);
      CHECK_THROWS_AS((bson::deserializer{buf}.load<S>()), std::runtime_error);

      if(n >= 4)
      {
        auto fixed = buf;
        put_i32(fixed, 0, static_cast<std::int32_t>(n));
        CHECK_THROWS_AS((bson::deserializer{fixed}.load<S>()), std::runtime_error);
      }
    }
  }

  SUBCASE("nested, contiguous cursor")
  {
    const auto full = encode(outer_t{{5}, std::string(24, 'p')});
    for(std::size_t n = 0; n < full.size(); ++n)
    {
      std::vector<std::byte> buf{full.begin(), full.begin() + static_cast<std::ptrdiff_t>(n)};
      CAPTURE(n);
      CHECK_THROWS_AS((bson::deserializer{buf}.load<outer_t>()), std::runtime_error);

      if(n >= 4)
      {
        auto fixed = buf;
        put_i32(fixed, 0, static_cast<std::int32_t>(n));
        CHECK_THROWS_AS((bson::deserializer{fixed}.load<outer_t>()), std::runtime_error);
      }
    }
  }

  // The bounds check is compiled out for an unsized cursor, so read_byte()'s end check has to
  // carry this shape on its own.
  SUBCASE("flat, istreambuf cursor")
  {
    const auto full = encode(S{42, "Hello, world!", 3.14});
    for(std::size_t n = 0; n < full.size(); ++n)
    {
      const std::string raw{reinterpret_cast<char const*>(full.data()), n};
      std::istringstream in{raw, std::ios::binary};
      CAPTURE(n);
      CHECK_THROWS_AS((bson::deserializer{in}.load<S>()), std::runtime_error);
    }
  }
}

TEST_CASE("reflex::core::bson file roundtrip")
{
  const std::filesystem::path bson_path = "test.bson";
  const S                     expected  = {42, "Hello, world!", 3.14};

  {
    std::ofstream    out_file{bson_path, std::ios::binary};
    bson::serializer ser{out_file};
    ser.dump(expected);
  }

  {
    std::ifstream in_file{bson_path, std::ios::binary};
    const auto    value = bson::deserializer{in_file}.load<S>();
    CHECK_EQ(value, expected);
  }

  std::filesystem::remove(bson_path);
}

// A std::string_view member reads a run of the input instead of a copy.
//
// BSON is the one backend where that borrow is unconditional. It carries a string
// as its bytes with a count in front, so there is no escape and no decode, and
// every payload on a contiguous input is already a run of it. There is no value
// this has to refuse and so no runtime throw, which is not true of the three
// character backends.
//
// On a non-contiguous input there is nothing to point at and it is a compile
// error:
//
//   bson::deserializer{some_istream}.load<BorrowedValue>();
//
//   error: static assertion failed: std::basic_string_view<char> cannot be a BSON
//   string destination on this cursor: a borrowed read needs a contiguous input to
//   point at, and this deserializer reads a byte at a time (use std::string, or
//   deserialize from a contiguous input)
//
// Before this, the same member compiled and read back the freed bytes of the local
// the payload had been decoded into, with no diagnostic and no throw.
static_assert(not serde::detail::string_sink_c<std::string_view>);
static_assert(serde::detail::borrowed_string_sink_c<std::string_view>);
static_assert(serde::detail::string_sink_c<std::string>);
static_assert(not serde::detail::borrowed_string_sink_c<std::string>);

namespace
{
  struct BorrowedValue
  {
    std::string_view text;
    std::int32_t     n;
  };

  // Where `needle` starts inside `haystack`, as a pointer, so a borrow can be
  // checked by address rather than by content.
  const char* find_bytes(std::span<const std::byte> haystack, std::string_view needle)
  {
    const auto* const first = reinterpret_cast<char const*>(haystack.data());
    const std::string_view all{first, haystack.size()};
    const auto             at = all.find(needle);
    REQUIRE(at != std::string_view::npos);
    return first + at;
  }

  std::vector<std::byte> bson_bytes(auto const& value)
  {
    std::vector<std::byte> out;
    bson::serializer       ser{out};
    ser.dump(value);
    return out;
  }
} // namespace

TEST_CASE("reflex::serde::bson: a borrowed string destination")
{
  SUBCASE("points into the input rather than copying it")
  {
    // Held by name: the view read out of it points into it.
    const auto in = bson_bytes(BorrowedValue{"hello there", 7});
    const auto v  = bson::deserializer{in}.load<BorrowedValue>();

    CHECK_EQ(v.text, "hello there"sv);
    CHECK_EQ(v.n, 7);
    // Address, not content: comparing bytes would pass for a copy too, and the
    // dangling read this replaces had the right length.
    CHECK_EQ(v.text.data(), find_bytes(in, "hello there"));
  }

  SUBCASE("the view excludes the terminator the length counts")
  {
    // The off-by-one that content-based checks miss: a view one byte too long
    // still compares equal to the expected text under most spellings, because
    // the stray byte is a null.
    const auto in = bson_bytes(BorrowedValue{"abcd", 1});
    const auto v  = bson::deserializer{in}.load<BorrowedValue>();

    CHECK_EQ(v.text.size(), 4u);
    CHECK_EQ(v.text.back(), 'd');
    CHECK_EQ(*(v.text.data() + v.text.size()), '\0'); // the terminator, just past the view
  }

  SUBCASE("an embedded null survives")
  {
    // The case that tells a length-prefixed read apart from one that stopped at
    // the terminator. A BSON string may hold a null and a view represents it.
    const auto with_null = "a\0b"sv;
    REQUIRE_EQ(with_null.size(), 3u);

    const auto in = bson_bytes(BorrowedValue{with_null, 2});
    const auto v  = bson::deserializer{in}.load<BorrowedValue>();

    CHECK_EQ(v.text.size(), 3u);
    CHECK_EQ(v.text, with_null);
    CHECK_EQ(v.n, 2);
  }

  SUBCASE("an empty value is fine")
  {
    const auto in = bson_bytes(BorrowedValue{"", 3});
    const auto v  = bson::deserializer{in}.load<BorrowedValue>();
    CHECK(v.text.empty());
    CHECK_EQ(v.n, 3);
  }

  SUBCASE("a truncated document still throws")
  {
    auto in = bson_bytes(BorrowedValue{"hello there", 7});
    in.resize(in.size() / 2);
    CHECK_THROWS((bson::deserializer{in}.load<BorrowedValue>()));
  }

  SUBCASE("borrows from a char input as well as a byte one")
  {
    // contiguous_byte_or_char covers std::byte, char and unsigned char, and the
    // payload is read through char const* in every case.
    const auto        bytes = bson_bytes(BorrowedValue{"hello there", 7});
    const std::string in{reinterpret_cast<char const*>(bytes.data()), bytes.size()};

    const auto v = bson::deserializer{in}.load<BorrowedValue>();
    CHECK_EQ(v.text, "hello there"sv);
    CHECK_EQ(v.text.data(), in.data() + in.find("hello there"));
  }
}

TEST_CASE("reflex::serde::bson: an owning string destination is unaffected")
{
  struct Owned
  {
    std::string         text;
    heapless::string<8> small;

    bool operator==(Owned const& other) const = default;
  };

  const Owned            expected{"hello there", "abc"};
  std::vector<std::byte> out;
  bson::serializer       ser{out};
  ser.dump(expected);

  CHECK_EQ(bson::deserializer{out}.load<Owned>(), expected);
}

TEST_CASE("reflex::serde::bson: an optional member")
{
  const auto encode = [](Optionals const& value) {
    std::vector<std::byte> out;
    bson::serializer       ser{out};
    ser.dump(value);
    return out;
  };

  SUBCASE("engaged round-trips")
  {
    const Optionals expected{9, "nine"};
    CHECK_EQ(bson::deserializer{encode(expected)}.load<Optionals>(), expected);
  }

  SUBCASE("empty round-trips")
  {
    const Optionals expected{};
    CHECK_EQ(bson::deserializer{encode(expected)}.load<Optionals>(), expected);
  }

  SUBCASE("an empty member is written as a BSON null, not omitted")
  {
    const auto bytes = encode(Optionals{});
    CHECK_EQ(bson::deserializer{bytes}.load<bson::value>().as<bson::object>().size(), 2u);
    CHECK(bson::deserializer{bytes}.load<bson::value>().as<bson::object>().at("number").is_null());
  }
}
