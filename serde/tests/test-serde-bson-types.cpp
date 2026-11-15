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

bytes cat(bytes a, bytes const& b)
{
  a.insert(a.end(), b.begin(), b.end());
  return a;
}

bytes str(std::string_view text)
{
  bytes out;
  put_i32(out, static_cast<std::int32_t>(text.size() + 1));
  put_text(out, text);
  put(out, {0});
  return out;
}

bytes cstr(std::string_view text)
{
  bytes out;
  put_text(out, text);
  put(out, {0});
  return out;
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

bytes oid_bytes()
{
  bytes out;
  for(int i = 1; i <= 12; ++i)
  {
    put(out, {i});
  }
  return out;
}

bson::object_id oid()
{
  bson::object_id id;
  for(std::size_t i = 0; i < id.bytes.size(); ++i)
  {
    id.bytes[i] = static_cast<std::byte>(i + 1);
  }
  return id;
}

bytes scope_doc()
{
  bytes x;
  put_i32(x, 1);
  return doc_of(element(0x10, "x", x));
}

bytes cws(std::int32_t total, bytes const& body)
{
  bytes out;
  put_i32(out, total);
  return cat(out, body);
}

bytes cws_of(std::string_view code, bytes const& scope)
{
  const bytes body = cat(str(code), scope);
  return cws(static_cast<std::int32_t>(body.size() + 4), body);
}

template <typename T> struct holder
{
  T v;

  bool operator==(holder const&) const = default;
};

struct tag_members
{
  bson::min_key                  lo;
  bson::max_key                  hi;
  bson::undefined                u;
  std::optional<bson::undefined> some;
  std::optional<bson::undefined> none;

  bool operator==(tag_members const&) const = default;
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

template <typename T> void check_type(T const& x, int type, bytes const& payload)
{
  const bytes keyed   = doc_of(element(type, "v", payload));
  const bytes wrapped = doc_of(element(type, "value", payload));

  CHECK(dump(bson::value{bson::object{{"v", bson::value{x}}}}) == keyed);
  CHECK(dump(holder<T>{x}) == keyed);
  CHECK(dump(x) == wrapped);

  for(bool from_stream : {false, true})
  {
    CAPTURE(from_stream);
    const auto generic = load<bson::value>(keyed, from_stream);
    CHECK(generic.template as<bson::object>().at("v").template as<T>() == x);
    CHECK(load<holder<T>>(keyed, from_stream).v == x);
    CHECK(load<T>(wrapped, from_stream) == x);
    CHECK(load<bson::value>(wrapped, from_stream).template as<T>() == x);
  }
}
} // namespace

TEST_CASE("reflex::serde::bson: every spec type round-trips through its spec encoding")
{
  SUBCASE("object_id")
  {
    check_type(oid(), 0x07, oid_bytes());
  }
  SUBCASE("timestamp, increment first")
  {
    bytes payload;
    put(payload, {0x04, 0x03, 0x02, 0x01, 0x0D, 0x0C, 0x0B, 0x0A});
    check_type(bson::timestamp{.increment = 0x01020304, .seconds = 0x0A0B0C0D}, 0x11, payload);
  }
  SUBCASE("regex")
  {
    check_type(bson::regex{"^a.*", "im"}, 0x0B, cat(cstr("^a.*"), cstr("im")));
    check_type(bson::regex{}, 0x0B, cat(cstr(""), cstr("")));
  }
  SUBCASE("javascript")
  {
    check_type(bson::javascript{"x = 1"}, 0x0D, str("x = 1"));
  }
  SUBCASE("symbol")
  {
    check_type(bson::symbol{"sym"}, 0x0E, str("sym"));
  }
  SUBCASE("db_pointer")
  {
    check_type(bson::db_pointer{"db.coll", oid()}, 0x0C, cat(str("db.coll"), oid_bytes()));
  }
  SUBCASE("min_key")
  {
    check_type(bson::min_key{}, 0xFF, {});
  }
  SUBCASE("max_key")
  {
    check_type(bson::max_key{}, 0x7F, {});
  }
  SUBCASE("undefined")
  {
    check_type(bson::undefined{}, 0x06, {});
  }
  SUBCASE("code_w_scope, scope kept byte-exact")
  {
    check_type(bson::code_w_scope{"x + 1", scope_doc()}, 0x0F, cws_of("x + 1", scope_doc()));
    check_type(bson::code_w_scope{"", doc_of({})}, 0x0F, cws_of("", doc_of({})));
  }
  SUBCASE("code_w_scope, default scope is the empty document")
  {
    CHECK_EQ(bson::code_w_scope{}.scope, doc_of({}));
    check_type(bson::code_w_scope{}, 0x0F, cws_of("", doc_of({})));
  }
}

TEST_CASE("reflex::serde::bson: empty tag types are values, not empty documents")
{
  const tag_members in{
      .lo = {}, .hi = {}, .u = {}, .some = bson::undefined{}, .none = std::nullopt};
  const bytes expected = doc_of(
      cat(cat(cat(element(0xFF, "lo", {}), element(0x7F, "hi", {})), element(0x06, "u", {})),
          cat(element(0x06, "some", {}), element(0x0A, "none", {}))));

  CHECK(dump(in) == expected);
  for(bool from_stream : {false, true})
  {
    CAPTURE(from_stream);
    const auto back = load<tag_members>(expected, from_stream);
    CHECK(back == in);
    CHECK(back.some.has_value());
    CHECK_FALSE(back.none.has_value());
  }
}

TEST_CASE("reflex::serde::bson: spec types reject a mismatched or truncated element")
{
  for(bool from_stream : {false, true})
  {
    CAPTURE(from_stream);
    CHECK_EQ(
        load_error<holder<bson::object_id>>(doc_of(element(0x02, "v", str("x"))), from_stream),
        "Expected BSON ObjectId type");
    CHECK_EQ(
        load_error<holder<bson::min_key>>(doc_of(element(0x7F, "v", {})), from_stream),
        "Expected BSON MinKey type");
    CHECK_EQ(
        load_error<holder<bson::undefined>>(doc_of(element(0x0A, "v", {})), from_stream),
        "Expected BSON undefined type");

    bytes cut;
    put(cut, {1, 2, 3});
    const bytes truncated = element(0x07, "v", cut);
    bytes       doc;
    put_i32(doc, static_cast<std::int32_t>(truncated.size() + 4));
    doc.insert(doc.end(), truncated.begin(), truncated.end());
    CHECK_EQ(load_error<holder<bson::object_id>>(doc, from_stream), "Unexpected end of BSON input");
    CHECK_EQ(load_error<bson::value>(doc, from_stream), "Unexpected end of BSON input");
  }
}

TEST_CASE("reflex::serde::bson: a regex string holding a null byte is not written")
{
  using namespace std::string_literals;
  CHECK_THROWS_WITH_AS(
      dump(bson::regex{"a\0b"s, ""}), "BSON regex pattern must not contain a null byte",
      std::invalid_argument);
  CHECK_THROWS_WITH_AS(
      dump(
          holder<bson::regex>{
              {"a", "i\0"s}
  }),
      "BSON regex options must not contain a null byte", std::invalid_argument);
}

TEST_CASE("reflex::serde::bson: code_w_scope reads through the skipper's length checks")
{
  const auto holder_error = [](bytes const& payload, bool from_stream) {
    return load_error<holder<bson::code_w_scope>>(doc_of(element(0x0F, "v", payload)), from_stream);
  };
  const auto value_error = [](bytes const& payload, bool from_stream) {
    return load_error<bson::value>(doc_of(element(0x0F, "v", payload)), from_stream);
  };

  bytes unterminated  = scope_doc();
  unterminated.back() = std::byte{0x01};

  const std::array<std::pair<bytes, std::string_view>, 5> cases{
      {
       {cws(13, cat(str(""), doc_of({}))), "Invalid BSON code with scope length"},
       {cws(16, cat(cat(str(""), doc_of({})), bytes(2, std::byte{0}))),
           "BSON code with scope length does not match its content"},
       {cws(14, cat(str("abcde"), doc_of({}))), "BSON element exceeds declared size"},
       {cws_of("abc", bytes{std::byte{4}, std::byte{0}, std::byte{0}, std::byte{0}}),
           "Invalid BSON document length"},
       {cws_of("x", unterminated), "BSON document missing terminator"},
       }
  };

  for(bool from_stream : {false, true})
  {
    CAPTURE(from_stream);
    for(auto const& [payload, message] : cases)
    {
      CAPTURE(message);
      CHECK_EQ(holder_error(payload, from_stream), message);
      CHECK_EQ(value_error(payload, from_stream), message);
    }
  }
}

TEST_CASE("reflex::serde::bson: code_w_scope is written only with an encoded scope")
{
  CHECK_THROWS_WITH_AS(
      dump(bson::code_w_scope{"x", {}}), "BSON code with scope needs an encoded scope document",
      std::invalid_argument);

  bytes short_prefix = scope_doc();
  short_prefix[0]    = std::byte{5};
  CHECK_THROWS_WITH_AS(
      dump(bson::code_w_scope{"x", short_prefix}),
      "BSON code with scope length prefix does not match its scope", std::invalid_argument);

  bytes unterminated  = scope_doc();
  unterminated.back() = std::byte{0x01};
  CHECK_THROWS_WITH_AS(
      dump(bson::code_w_scope{"x", unterminated}),
      "BSON code with scope needs an encoded scope document", std::invalid_argument);
}
