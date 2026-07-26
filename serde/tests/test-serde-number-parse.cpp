#include <doctest/doctest.h>

import reflex.serde.csv;
import reflex.serde.json;
import reflex.serde.xml;

import std;

using namespace reflex;
using namespace reflex::serde;
using namespace std::literals;

// Every backend requires a number to consume the whole text it is parsed from.
// These paths used to disagree: both backends against reflex::parse, a plain
// column against a Parse deriving one, and JSON's two cursors against each
// other. reflex::parse and parse_strict themselves are covered in
// core/tests/test-parse.cpp.

namespace
{
  // Reaches CSV through the derives_c<Parse> arm rather than the number arm, so
  // the two columns below exercise different code for the same bytes. Format is
  // what makes it a CSV cell at all, Parse is what reads it back.
  struct[[= derive(Format, Parse)]] Ratio
  {
    double value{};
    constexpr bool operator==(Ratio const&) const = default;
  };

  constexpr parse_result<Ratio> tag_invoke(
      tag_t<Parse>, std::string_view s, std::type_identity<Ratio>) noexcept
  {
    auto inner = reflex::parse<double>(s);
    if(not inner)
    {
      return std::unexpected(std::errc::invalid_argument);
    }
    // Forwarding the inner end() is what makes parse_strict able to see leftover
    // input. Reporting s.data() + s.size() would claim the whole cell was
    // consumed and defeat it.
    return {Ratio{*inner}, inner.end()};
  }

  struct Numbers
  {
    double plain;
    Ratio  derived;
  };
}

template <> struct std::formatter<Ratio> : std::formatter<double>
{
  auto format(Ratio const& r, auto& ctx) const
  {
    return std::formatter<double>::format(r.value, ctx);
  }
};

namespace
{
  struct Scalar
  {
    double value;
  };

  template <typename T> T load_csv(std::string_view text)
  {
    return csv::deserializer{text}.load<T>();
  }

  template <typename T> T load_xml(std::string_view text)
  {
    return xml::deserializer{text}.load<T>();
  }

  // The contiguous cursor takes the from_chars fast path, the stream cursor does
  // not. Both must reach the same verdict.
  template <typename T> T load_json(std::string_view text)
  {
    return json::deserializer{text}.load<T>();
  }

  template <typename T> T load_json_streaming(std::string_view text)
  {
    std::istringstream in{std::string{text}};
    return json::deserializer{
        std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}}
        .template load<T>();
  }
}

TEST_CASE("reflex::serde: the backends agree with reflex::parse on trailing garbage")
{
  SUBCASE("csv")
  {
    CHECK_EQ(load_csv<Scalar>("value\r\n1.5\r\n").value, 1.5);
    CHECK_THROWS(load_csv<Scalar>("value\r\n1.5abc\r\n"));
    // No trim on a cell, unlike XML element text.
    CHECK_THROWS(load_csv<Scalar>("value\r\n1.5 \r\n"));
  }

  SUBCASE("xml")
  {
    CHECK_EQ(load_xml<Scalar>("<Scalar><value>1.5</value></Scalar>").value, 1.5);
    CHECK_THROWS(load_xml<Scalar>("<Scalar><value>1.5abc</value></Scalar>"));
    // Element text is trimmed first, so surrounding space is not garbage.
    CHECK_EQ(load_xml<Scalar>("<Scalar><value> 1.5 </value></Scalar>").value, 1.5);
  }

  SUBCASE("a plain column and a Parse deriving one apply the same rule")
  {
    const auto ok = load_csv<Numbers>("plain,derived\r\n1.5,2.5\r\n");
    CHECK_EQ(ok.plain, 1.5);
    CHECK_EQ(ok.derived, Ratio{2.5});

    CHECK_THROWS(load_csv<Numbers>("plain,derived\r\n1.5abc,2.5\r\n"));
    CHECK_THROWS(load_csv<Numbers>("plain,derived\r\n1.5,2.5abc\r\n"));
  }
}

TEST_CASE("reflex::serde::json: both cursors reject the same number")
{
  CHECK_EQ(load_json<double>("1.2"), 1.2);
  CHECK_EQ(load_json_streaming<double>("1.2"), 1.2);

  // Used to yield 1.2 and leave ".3" for the next token on the contiguous path,
  // and 1.2 with all five bytes consumed on the stream path.
  CHECK_THROWS(load_json<double>("1.2.3"));
  CHECK_THROWS(load_json_streaming<double>("1.2.3"));

  CHECK_THROWS(load_json<double>("1.5abc"));
  CHECK_THROWS(load_json_streaming<double>("1.5abc"));

  // A number inside a document ends at a structural character, which is not
  // trailing garbage.
  CHECK_EQ(load_json<std::vector<int>>("[1,2,3]").size(), 3u);
  CHECK_EQ(load_json<Scalar>("{\"value\": 1.5 }").value, 1.5);
  CHECK_EQ(load_json_streaming<Scalar>("{\"value\": 1.5 }").value, 1.5);
}
