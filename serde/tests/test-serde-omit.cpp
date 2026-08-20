#include <doctest/doctest.h>
#include <reflex/const_check.hpp>

import reflex.serde;
import reflex.serde.json;
import reflex.serde.bson;

import std;

using namespace reflex;
using namespace reflex::serde;

namespace
{
  struct plain
  {
    std::optional<int>         opt;
    std::vector<int>           seq;
    std::string                text;
    std::map<std::string, int> table;
    int                        scalar;
  };

  struct annotated_members
  {
    [[= serde::omit_if_empty{}]] std::optional<int>         opt;
    [[= serde::omit_if_empty{}]] std::vector<int>           seq;
    [[= serde::omit_if_empty{}]] std::string                text;
    [[= serde::omit_if_empty{}]] std::map<std::string, int> table;
    [[= serde::omit_if_empty{}]] std::array<char, 4>        fixed;
    int                                                     scalar;
  };

  struct[[= serde::omit_if_empty{}]] annotated_type
  {
    std::optional<int> opt;
    std::vector<int>   seq;
    int                scalar;
  };

  struct[[= serde::omit_if_empty{^^std::optional}]] filtered_to_optional
  {
    std::optional<int> opt;
    std::vector<int>   seq;
    std::string        text;
  };

  struct[[= serde::omit_if_empty{^^std::string}]] filtered_to_string
  {
    std::optional<int> opt;
    std::string        text;
  };

  struct[[= serde::omit_if_empty{^^std::vector, ^^std::map}]] filtered_to_containers
  {
    std::vector<int>           seq;
    std::map<std::string, int> table;
    std::string                text;
  };

  struct[[= serde::omit_if_empty{}]] cancelled
  {
    [[= serde::no_omit]] std::optional<int> opt;
    std::optional<int>                      other;
  };

  struct bad_scalar
  {
    [[= serde::omit_if_empty{}]] int scalar;
  };

  struct bad_fixed_array
  {
    [[= serde::omit_if_empty{}]] std::array<int, 3> fixed;
  };

  struct bad_filter
  {
    [[= serde::omit_if_empty{^^std::vector}]] std::string text;
  };

  struct[[= serde::no_omit]] no_omit_on_type
  {
    std::optional<int> opt;
  };

  template <typename T> consteval bool omits(std::size_t index)
  {
    return omits_when_empty(
        nonstatic_data_members_of(^^T, std::meta::access_context::current())[index]);
  }

  static_assert(not omits<plain>(0));
  static_assert(not omits<plain>(1));
  static_assert(not omits<plain>(2));
  static_assert(not omits<plain>(3));
  static_assert(not omits<plain>(4));

  static_assert(omits<annotated_members>(0));
  static_assert(omits<annotated_members>(1));
  static_assert(omits<annotated_members>(2));
  static_assert(omits<annotated_members>(3));
  static_assert(omits<annotated_members>(4));
  static_assert(not omits<annotated_members>(5));

  static_assert(omits<annotated_type>(0));
  static_assert(omits<annotated_type>(1));
  static_assert(not omits<annotated_type>(2));

  static_assert(omits<filtered_to_optional>(0));
  static_assert(not omits<filtered_to_optional>(1));
  static_assert(not omits<filtered_to_optional>(2));

  static_assert(not omits<filtered_to_string>(0));
  static_assert(omits<filtered_to_string>(1));

  static_assert(omits<filtered_to_containers>(0));
  static_assert(omits<filtered_to_containers>(1));
  static_assert(not omits<filtered_to_containers>(2));

  static_assert(not omits<cancelled>(0));
  static_assert(omits<cancelled>(1));

  consteval {
    REFLEX_CONSTEVAL_THROWS_WITH("can never be empty", omits<bad_scalar>(0));
    REFLEX_CONSTEVAL_THROWS_WITH("can never be empty", omits<bad_fixed_array>(0));
    REFLEX_CONSTEVAL_THROWS_WITH("lists no type matching", omits<bad_filter>(0));
    REFLEX_CONSTEVAL_THROWS_WITH("belongs on a member", omits<no_omit_on_type>(0));
    REFLEX_CONSTEVAL_THROWS_WITH("a type or a template", omit_if_empty{^^::});
    REFLEX_CONSTEVAL_NOTHROW(omit_if_empty{^^std::optional, ^^std::string});
  }
}

TEST_CASE("an empty value is recognised whatever shape it takes")
{
  CHECK(is_empty_value(std::optional<int>{}));
  CHECK_FALSE(is_empty_value(std::optional<int>{0}));

  CHECK(is_empty_value(std::vector<int>{}));
  CHECK_FALSE(is_empty_value(std::vector<int>{0}));

  CHECK(is_empty_value(std::string{}));
  CHECK_FALSE(is_empty_value(std::string{"x"}));

  CHECK(is_empty_value(std::string_view{}));
  CHECK_FALSE(is_empty_value(std::string_view{"x"}));

  CHECK(is_empty_value(std::map<std::string, int>{}));
  CHECK_FALSE(is_empty_value(std::map<std::string, int>{{"k", 1}}));

  CHECK(is_empty_value(std::array<char, 4>{}));
  CHECK_FALSE(is_empty_value(std::array<char, 4>{'a', 'b'}));

  const char* null_text = nullptr;
  const char* no_text   = "";
  const char* some_text = "x";
  CHECK(is_empty_value(null_text));
  CHECK(is_empty_value(no_text));
  CHECK_FALSE(is_empty_value(some_text));
}

namespace
{
  struct[[= serde::omit_if_empty{}]] doc
  {
    std::optional<int>         opt;
    std::vector<int>           seq;
    std::string                text;
    std::map<std::string, int> table;
    int                        scalar;
  };

  struct spread
  {
    [[= serde::omit_if_empty{}]] std::string head;
    int                                      middle;
    [[= serde::omit_if_empty{}]] std::string tail;
  };

  struct[[= serde::omit_if_empty{}]] all_omittable
  {
    std::optional<int> opt;
    std::vector<int>   seq;
  };

  template <typename T> std::string to_json(T const& value)
  {
    std::string      out;
    json::serializer ser{out};
    serialize(ser, value);
    return out;
  }
}

TEST_CASE("JSON leaves an annotated empty field out")
{
  CHECK(to_json(doc{}) == R"({"scalar":0})");
  CHECK(to_json(doc{.opt = 7, .seq = {1}, .text = "x", .table = {{"k", 2}}, .scalar = 3})
        == R"({"opt":7,"seq":[1],"text":"x","table":{"k":2},"scalar":3})");
}

TEST_CASE("JSON keeps an unannotated empty field, which is what makes this opt-in")
{
  CHECK(to_json(plain{}) == R"({"opt":null,"seq":[],"text":"","table":{},"scalar":0})");
}

TEST_CASE("JSON writes no stray separator around an omitted field")
{
  CHECK(to_json(spread{}) == R"({"middle":0})");
  CHECK(to_json(spread{.head = "h", .middle = 1, .tail = {}}) == R"({"head":"h","middle":1})");
  CHECK(to_json(spread{.head = {}, .middle = 1, .tail = "t"}) == R"({"middle":1,"tail":"t"})");
  CHECK(to_json(all_omittable{}) == "{}");
}

TEST_CASE("JSON round-trips a document whose empty fields were omitted")
{
  auto source   = doc{};
  source.scalar = 5;

  const auto text = to_json(source);
  auto       back = json::deserializer{text}.load<doc>();
  CHECK(back.opt == std::nullopt);
  CHECK(back.seq.empty());
  CHECK(back.text.empty());
  CHECK(back.table.empty());
  CHECK(back.scalar == 5);
}

namespace
{
  struct nested_holder
  {
    [[= serde::omit_if_empty{}]] std::string outer;
    doc                                      inner;
  };

  template <typename T> std::vector<std::byte> to_bson(T const& value)
  {
    std::vector<std::byte> out;
    bson::serializer       ser{out};
    serialize(ser, value);
    return out;
  }

  static_assert(omits_when_empty(
      nonstatic_data_members_of(^^nested_holder, std::meta::access_context::current())[0]));

  bool bson_has_key(std::vector<std::byte> const& document, std::string_view key)
  {
    const auto bytes = std::string_view{reinterpret_cast<char const*>(document.data()),
                                        document.size()};
    return bytes.find(key) != std::string_view::npos;
  }
}

TEST_CASE("BSON leaves an annotated empty field out")
{
  const auto document = to_bson(doc{});
  CHECK_FALSE(bson_has_key(document, "opt"));
  CHECK_FALSE(bson_has_key(document, "seq"));
  CHECK_FALSE(bson_has_key(document, "text"));
  CHECK_FALSE(bson_has_key(document, "table"));
  CHECK(bson_has_key(document, "scalar"));
  CHECK(document.size() < to_bson(plain{}).size());

  const auto filled = to_bson(doc{.opt = 7, .seq = {1}, .text = "x", .table = {{"k", 2}},
                                  .scalar = 3});
  CHECK(bson_has_key(filled, "opt"));
  CHECK(bson_has_key(filled, "seq"));
  CHECK(bson_has_key(filled, "text"));
  CHECK(bson_has_key(filled, "table"));
}

TEST_CASE("BSON keeps an unannotated empty field, which is what makes this opt-in")
{
  const auto document = to_bson(plain{});
  CHECK(bson_has_key(document, "opt"));
  CHECK(bson_has_key(document, "seq"));
  CHECK(bson_has_key(document, "text"));
  CHECK(bson_has_key(document, "table"));
}

TEST_CASE("BSON omits inside a nested document, not only at the root")
{
  const auto document = to_bson(nested_holder{});
  CHECK_FALSE(bson_has_key(document, "outer"));
  CHECK(bson_has_key(document, "inner"));
  CHECK_FALSE(bson_has_key(document, "text"));
  CHECK(bson_has_key(document, "scalar"));
}

TEST_CASE("BSON round-trips a document whose empty fields were omitted")
{
  auto source   = doc{};
  source.scalar = 5;

  const auto document = to_bson(source);
  auto       back     = bson::deserializer{document}.load<doc>();
  CHECK(back.opt == std::nullopt);
  CHECK(back.seq.empty());
  CHECK(back.text.empty());
  CHECK(back.table.empty());
  CHECK(back.scalar == 5);
}
