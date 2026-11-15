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

void put_text(bytes& out, std::string_view text)
{
  for(char c : text)
  {
    out.push_back(static_cast<std::byte>(c));
  }
}

bytes str(std::string_view text)
{
  bytes out;
  put_i32(out, static_cast<std::int32_t>(text.size() + 1));
  put_text(out, text);
  put(out, {0});
  return out;
}

bytes cat(bytes a, bytes const& b)
{
  a.insert(a.end(), b.begin(), b.end());
  return a;
}

bytes doc_of(bytes const& elements)
{
  bytes out;
  put_i32(out, static_cast<std::int32_t>(elements.size() + 5));
  out.insert(out.end(), elements.begin(), elements.end());
  put(out, {0});
  return out;
}

bytes element(int type, std::string_view key, bytes const& payload)
{
  bytes out;
  put(out, {type});
  put_text(out, key);
  put(out, {0});
  out.insert(out.end(), payload.begin(), payload.end());
  return out;
}

bytes kept_element()
{
  bytes payload;
  put_i32(payload, 7);
  return element(0x10, "kept", payload);
}

bytes with_gone(int type, bytes const& payload)
{
  return doc_of(cat(element(type, "gone", payload), kept_element()));
}

bytes zeros(std::size_t n)
{
  return bytes(n, std::byte{0});
}

struct sk_target
{
  int kept;

  bool operator==(sk_target const&) const = default;
};

struct sk_nested
{
  sk_target inner;
};

constexpr bson::read_options skip_opts{.unknown_keys = unknown_keys::skip};

template <typename T> T load_skip(bytes const& data, bool from_stream)
{
  if(from_stream)
  {
    std::istringstream in{
        std::string{reinterpret_cast<char const*>(data.data()), data.size()}
    };
    return bson::deserializer{in, std::cw<skip_opts>}.template load<T>();
  }
  return bson::deserializer{data, std::cw<skip_opts>}.template load<T>();
}

template <typename T> std::string load_error(bytes const& data, bool from_stream, bool skip)
{
  try
  {
    if(skip)
    {
      (void)load_skip<T>(data, from_stream);
    }
    else if(from_stream)
    {
      std::istringstream in{
          std::string{reinterpret_cast<char const*>(data.data()), data.size()}
      };
      (void)bson::deserializer{in}.template load<T>();
    }
    else
    {
      (void)bson::deserializer{data}.template load<T>();
    }
  }
  catch(std::runtime_error const& e)
  {
    return e.what();
  }
  return {};
}

struct typed
{
  int   type;
  bytes payload;
};

bytes raw_doc(std::int32_t declared, bytes const& elements)
{
  bytes out;
  put_i32(out, declared);
  out.insert(out.end(), elements.begin(), elements.end());
  return out;
}

void expect_skip_error(bytes const& data, bool from_stream, std::string_view message)
{
  CAPTURE(from_stream);
  CHECK_EQ(load_error<sk_target>(data, from_stream, true), message);
}

bytes cws(std::int32_t total, bytes const& inner)
{
  bytes out;
  put_i32(out, total);
  out.insert(out.end(), inner.begin(), inner.end());
  return out;
}
} // namespace

TEST_CASE("reflex::serde::bson: unknown_keys::skip steps over every element type")
{
  bytes i32;
  put_i32(i32, 1);

  bytes binary;
  put_i32(binary, 3);
  put(binary, {0x80, 1, 2, 3});

  bytes regex;
  put_text(regex, "a.c");
  put(regex, {0});
  put_text(regex, "im");
  put(regex, {0});

  bytes db_pointer = cat(str("coll"), zeros(12));

  bytes scope = doc_of(cat(element(0x10, "x", i32), element(0x04, "arr", doc_of({}))));
  bytes code_w_scope =
      cws(static_cast<std::int32_t>(4 + str("f()").size() + scope.size()), cat(str("f()"), scope));

  bytes nested = doc_of(
      cat(element(0x04, "list", doc_of(element(0x03, "0", doc_of({})))),
          element(0x02, "s", str("deep"))));

  const std::vector<typed> cases{
      {0x0A, {}},
      {0x06, {}},
      {0xFF, {}},
      {0x7F, {}},
      {0x08, bytes(1, std::byte{1})},
      {0x10, zeros(4)},
      {0x01, zeros(8)},
      {0x12, zeros(8)},
      {0x09, zeros(8)},
      {0x11, zeros(8)},
      {0x07, zeros(12)},
      {0x13, zeros(16)},
      {0x02, str("hello")},
      {0x0D, str("code")},
      {0x0E, str("sym")},
      {0x05, binary},
      {0x03, nested},
      {0x04, nested},
      {0x0F, code_w_scope},
      {0x0C, db_pointer},
      {0x0B, regex},
  };

  for(bool from_stream : {false, true})
  {
    for(auto const& c : cases)
    {
      CAPTURE(c.type);
      CAPTURE(from_stream);
      CHECK_EQ(load_skip<sk_target>(with_gone(c.type, c.payload), from_stream), sk_target{7});
      CHECK_EQ(
          load_error<sk_target>(with_gone(c.type, c.payload), from_stream, false),
          "BSON: no member matches the key 'gone'");
    }
  }
}

TEST_CASE("reflex::serde::bson: unknown_keys::skip positions")
{
  bytes i32;
  put_i32(i32, 1);

  for(bool from_stream : {false, true})
  {
    CAPTURE(from_stream);

    SUBCASE("the unknown key last")
    {
      auto data = doc_of(cat(kept_element(), element(0x10, "gone", i32)));
      CHECK_EQ(load_skip<sk_target>(data, from_stream), sk_target{7});
    }

    SUBCASE("two unknown keys in a row")
    {
      auto data =
          doc_of(cat(cat(element(0x10, "a", i32), element(0x02, "b", str("x"))), kept_element()));
      CHECK_EQ(load_skip<sk_target>(data, from_stream), sk_target{7});
    }

    SUBCASE("inside a nested aggregate member")
    {
      auto inner = with_gone(0x02, str("zz"));
      auto data  = doc_of(element(0x03, "inner", inner));
      CHECK_EQ(load_skip<sk_nested>(data, from_stream).inner, sk_target{7});
    }

    SUBCASE("the error mode names a nested key")
    {
      auto inner = with_gone(0x02, str("zz"));
      auto data  = doc_of(element(0x03, "inner", inner));
      CHECK_EQ(
          load_error<sk_nested>(data, from_stream, false),
          "BSON: no member matches the key 'gone'");
    }
  }
}

TEST_CASE("reflex::serde::bson: unknown_keys::skip rejects hostile lengths")
{
  const auto fails = [](int type, bytes const& payload, std::string_view message) {
    CAPTURE(type);
    for(bool from_stream : {false, true})
    {
      expect_skip_error(with_gone(type, payload), from_stream, message);
    }
  };

  bytes p;

  put_i32(p, 0);
  fails(0x02, p, "Invalid BSON string length");

  p.clear();
  put_i32(p, -5);
  put(p, {0, 0, 0});
  fails(0x02, p, "Invalid BSON string length");

  p.clear();
  put_i32(p, 100);
  put(p, {'a', 0});
  fails(0x02, p, "Unexpected end of BSON input");

  p.clear();
  put_i32(p, 3);
  put(p, {'a', 'b', 'c'});
  fails(0x02, p, "BSON string payload must be null-terminated");

  p.clear();
  put_i32(p, 4);
  fails(0x03, p, "Invalid BSON document length");

  p.clear();
  put_i32(p, -1);
  put(p, {0});
  fails(0x05, p, "Invalid BSON binary length");

  fails(0x0F, cws(13, {}), "Invalid BSON code with scope length");

  fails(
      0x0F, cws(14, cat(str(std::string(100, 'x')), doc_of({}))),
      "BSON element exceeds declared size");

  {
    bytes lying;
    put_i32(lying, 1000000);
    put(lying, {'a', 0});
    fails(0x0F, cws(14, lying), "BSON element exceeds declared size");
  }

  {
    bytes scope = doc_of({});
    fails(
        0x0F,
        cws(static_cast<std::int32_t>(4 + str("f").size() + scope.size() + 2),
            cat(str("f"), scope)),
        "BSON code with scope length does not match its content");
  }

  fails(0x42, {}, "BSON: unknown element type 0x42");
}

TEST_CASE("reflex::serde::bson: a skipped code with scope whose string ends at its limit")
{
  for(std::string_view text : {"abc", "abcd", "abcde"})
  {
    CAPTURE(text);
    for(bool from_stream : {false, true})
    {
      expect_skip_error(
          with_gone(0x0F, cws(14, cat(str(text), doc_of({})))), from_stream,
          "BSON element exceeds declared size");
    }
  }
}

TEST_CASE("reflex::serde::bson: a skipped element cut by the end of the input")
{
  SUBCASE("regex")
  {
    bytes payload;
    put(payload, {'a', 0, 'b'});
    expect_skip_error(
        raw_doc(100, element(0x0B, "gone", payload)), true, "Unexpected end of BSON input");

    const bytes cut = element(0x0B, "gone", payload);
    for(bool from_stream : {false, true})
    {
      expect_skip_error(
          raw_doc(static_cast<std::int32_t>(cut.size() + 4), cut), from_stream,
          "Unexpected end of BSON input");
    }
  }

  SUBCASE("db_pointer")
  {
    bytes payload = str("coll");
    put(payload, {1, 2, 3});
    expect_skip_error(
        raw_doc(100, element(0x0C, "gone", payload)), true, "Unexpected end of BSON input");

    const bytes cut = element(0x0C, "gone", payload);
    for(bool from_stream : {false, true})
    {
      expect_skip_error(
          raw_doc(static_cast<std::int32_t>(cut.size() + 4), cut), from_stream,
          "Unexpected end of BSON input");
    }
  }

  SUBCASE("string claiming a million bytes")
  {
    bytes p;
    put_i32(p, 1000000);
    put(p, {'a', 0});
    expect_skip_error(with_gone(0x02, p), true, "Unexpected end of BSON input");
  }
}

TEST_CASE("reflex::serde::bson: a skipped string overrunning its parent document")
{
  const bytes elements = element(0x02, "gone", str(std::string(25, 'x')));
  const bytes inner =
      raw_doc(static_cast<std::int32_t>(elements.size() + 5 - 10), cat(elements, zeros(1)));
  const bytes data = doc_of(element(0x03, "inner", inner));

  for(bool from_stream : {false, true})
  {
    CAPTURE(from_stream);
    CHECK_EQ(
        load_error<sk_nested>(data, from_stream, true),
        "BSON document element exceeds declared size");
  }
}
