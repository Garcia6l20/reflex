#include <doctest/doctest.h>

import reflex.serde.bson;

import std;

using namespace reflex;
using namespace reflex::serde;

namespace
{
using bytes = std::vector<std::byte>;

void put(bytes& out, std::initializer_list<int> list)
{
  for(int b : list)
  {
    out.push_back(static_cast<std::byte>(b));
  }
}

void put_i32(bytes& out, std::int32_t v)
{
  for(int i = 0; i < 4; ++i)
  {
    out.push_back(static_cast<std::byte>((static_cast<std::uint32_t>(v) >> (8 * i)) & 0xFFu));
  }
}

bytes cat(bytes a, bytes const& b)
{
  a.insert(a.end(), b.begin(), b.end());
  return a;
}

bytes i32(std::int32_t v)
{
  bytes out;
  put_i32(out, v);
  return out;
}

bytes doc_of(bytes const& elements)
{
  return cat(cat(i32(static_cast<std::int32_t>(elements.size() + 5)), elements), bytes(1));
}

bytes element(std::string_view key, bytes const& payload)
{
  bytes out;
  put(out, {0x05});
  for(char c : key)
  {
    out.push_back(static_cast<std::byte>(c));
  }
  put(out, {0});
  return cat(out, payload);
}

bytes binary_payload(std::int32_t total, int subtype, bytes const& rest)
{
  bytes out = i32(total);
  put(out, {subtype});
  return cat(out, rest);
}

bytes sample()
{
  bytes out;
  put(out, {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x7F});
  return out;
}

struct holder
{
  bson::binary v;

  bool operator==(holder const&) const = default;
};

struct prefilled_holder
{
  bson::binary v{.subtype = std::byte{0x80}, .data = {std::byte{0x01}}};
};

struct byte_member
{
  std::vector<std::byte> v;
};

template <typename T> bytes dump(T const& value)
{
  bytes            out;
  bson::serializer ser{out};
  ser.dump(value);
  return out;
}

template <typename T> T load(bytes const& data, bool from_stream)
{
  if(from_stream)
  {
    std::istringstream in{
        std::string{reinterpret_cast<char const*>(data.data()), data.size()}
    };
    return bson::deserializer{in}.template load<T>();
  }
  return bson::deserializer{data}.template load<T>();
}

template <typename T> std::string load_error(bytes const& data, bool from_stream)
{
  try
  {
    (void)load<T>(data, from_stream);
  }
  catch(std::runtime_error const& e)
  {
    return e.what();
  }
  return {};
}

void check_binary(bson::binary const& x, bytes const& payload)
{
  const bytes keyed   = doc_of(element("v", payload));
  const bytes wrapped = doc_of(element("value", payload));

  CHECK(dump(bson::value{bson::object{{"v", bson::value{x}}}}) == keyed);
  CHECK(dump(holder{x}) == keyed);
  CHECK(dump(x) == wrapped);

  for(bool from_stream : {false, true})
  {
    CAPTURE(from_stream);
    const auto generic = load<bson::value>(keyed, from_stream);
    CHECK(generic.as<bson::object>().at("v").as<bson::binary>() == x);
    CHECK(load<holder>(keyed, from_stream).v == x);
    CHECK(load<bson::binary>(wrapped, from_stream) == x);
    CHECK(load<bson::value>(wrapped, from_stream).as<bson::binary>() == x);
    CHECK(dump(generic) == keyed);
  }
}
}

TEST_CASE("reflex::serde::bson: bson::binary carries any subtype")
{
  for(int subtype : {0x00, 0x04, 0x05, 0x80})
  {
    CAPTURE(subtype);
    const auto st = static_cast<std::byte>(subtype);
    check_binary(
        bson::binary{.subtype = st, .data = sample()}, binary_payload(6, subtype, sample()));
    check_binary(bson::binary{.subtype = st, .data = {}}, binary_payload(0, subtype, {}));
  }
}

TEST_CASE("reflex::serde::bson: subtype 0x02 drops its inner length on read and rebuilds it")
{
  check_binary(
      bson::binary{.subtype = std::byte{0x02}, .data = sample()},
      binary_payload(10, 0x02, cat(i32(6), sample())));
  check_binary(
      bson::binary{.subtype = std::byte{0x02}, .data = {}}, binary_payload(4, 0x02, i32(0)));
}

TEST_CASE("reflex::serde::bson: subtype 0x02 rejects an inner length that does not match")
{
  const std::array<std::pair<bytes, std::string_view>, 3> cases{
      {
       {binary_payload(10, 0x02, cat(i32(7), sample())),
           "BSON binary subtype 0x02 inner length 7 does not match its 6 payload bytes"},
       {binary_payload(10, 0x02, cat(i32(-1), sample())),
           "BSON binary subtype 0x02 inner length -1 does not match its 6 payload bytes"},
       {binary_payload(2, 0x02, bytes(2)),
           "BSON binary subtype 0x02 is too short for its inner length"},
       }
  };

  for(bool from_stream : {false, true})
  {
    CAPTURE(from_stream);
    for(auto const& [payload, message] : cases)
    {
      CAPTURE(message);
      CHECK_EQ(load_error<holder>(doc_of(element("v", payload)), from_stream), message);
      CHECK_EQ(load_error<bson::value>(doc_of(element("v", payload)), from_stream), message);
    }
  }
}

TEST_CASE("reflex::serde::bson: a byte vector member still rejects a non-zero subtype")
{
  const bytes doc = doc_of(element("v", binary_payload(6, 0x04, sample())));
  for(bool from_stream : {false, true})
  {
    CAPTURE(from_stream);
    CHECK_EQ(load_error<byte_member>(doc, from_stream), "Unsupported BSON binary subtype: 0x04");
  }
}

TEST_CASE("reflex::serde::bson: a binary destination rejects another element type")
{
  bytes keyed = doc_of(element("v", i32(1)));
  keyed[4]    = std::byte{0x10};
  for(bool from_stream : {false, true})
  {
    CAPTURE(from_stream);
    CHECK_EQ(load_error<holder>(keyed, from_stream), "Expected BSON binary type");
  }
}

TEST_CASE("reflex::serde::bson: a binary member replaces its default contents")
{
  const bytes doc = doc_of(element("v", binary_payload(6, 0x04, sample())));
  for(bool from_stream : {false, true})
  {
    CAPTURE(from_stream);
    const auto back = load<prefilled_holder>(doc, from_stream);
    CHECK(back.v == bson::binary{.subtype = std::byte{0x04}, .data = sample()});
  }
}
