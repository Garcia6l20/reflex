#include <doctest/doctest.h>

import reflex.serde.xml;

import std;

using namespace reflex;
using namespace reflex::serde;
using namespace std::literals;

struct[[= serde::naming::camel_case, = derive(Debug)]] Basic
{
  int                                    int_member;
  std::string                            string_member;
  [[= serde::naming::kebab_case]] double double_member;

  constexpr bool operator==(Basic const& other) const = default;
};

enum class[[= derive(Format, Parse)]] Color
{
  Red,
  Green,
  Blue
};

struct[[= derive(Debug)]] Opt
{
  std::string        name;
  std::optional<int> count;

  constexpr bool operator==(Opt const& other) const = default;
};

struct[[= derive(Debug)]] Enumed
{
  std::string name;
  Color       color;

  constexpr bool operator==(Enumed const& other) const = default;
};

struct[[= derive(Debug)]] WithSeq
{
  std::string      name;
  std::vector<int> values;

  constexpr bool operator==(WithSeq const& other) const = default;
};

struct[[= derive(Debug)]] Inner
{
  int         a;
  std::string b;

  constexpr bool operator==(Inner const& other) const = default;
};

struct[[= derive(Debug)]] Outer
{
  std::string name;
  Inner       inner;

  constexpr bool operator==(Outer const& other) const = default;
};

TEST_CASE("reflex::serde::xml::serializer: single element")
{
  std::string     out;
  xml::serializer ser{out};
  ser.dump(Basic{42, "hello", 3.14});
  CHECK_EQ(
      out,
      "<Basic><intMember>42</intMember><stringMember>hello</stringMember>"
      "<double-member>3.14</double-member></Basic>");
}

TEST_CASE("reflex::serde::xml: escaping")
{
  const Basic value{7, "a & b < c > d", 0.0};
  std::string out;
  {
    xml::serializer ser{out};
    ser.dump(value);
  }
  CHECK(out.find("a &amp; b &lt; c &gt; d") != std::string::npos);
  const auto back = xml::deserializer{out}.load<Basic>();
  CHECK_EQ(back, value);
}

TEST_CASE("reflex::serde::xml: sequence member")
{
  SUBCASE("populated")
  {
    std::string out;
    {
      xml::serializer ser{out};
      ser.dump(WithSeq{"xs", {1, 2, 3}});
    }
    CHECK_EQ(
        out,
        "<WithSeq><name>xs</name><values>1</values><values>2</values>"
        "<values>3</values></WithSeq>");
    const auto back = xml::deserializer{out}.load<WithSeq>();
    CHECK_EQ(back, WithSeq{"xs", {1, 2, 3}});
  }
  SUBCASE("empty -> no elements")
  {
    std::string out;
    {
      xml::serializer ser{out};
      ser.dump(WithSeq{"xs", {}});
    }
    CHECK_EQ(out, "<WithSeq><name>xs</name></WithSeq>");
    const auto back = xml::deserializer{out}.load<WithSeq>();
    CHECK_EQ(back, WithSeq{"xs", {}});
  }
}

TEST_CASE("reflex::serde::xml: nested aggregate")
{
  const Outer value{"o", {5, "deep"}};
  std::string out;
  {
    xml::serializer ser{out};
    ser.dump(value);
  }
  CHECK_EQ(
      out,
      "<Outer><name>o</name><inner><a>5</a><b>deep</b></inner></Outer>");
  const auto back = xml::deserializer{out}.load<Outer>();
  CHECK_EQ(back, value);
}

TEST_CASE("reflex::serde::xml: optional member")
{
  SUBCASE("present")
  {
    std::string out;
    {
      xml::serializer ser{out};
      ser.dump(Opt{"a", 5});
    }
    CHECK_EQ(out, "<Opt><name>a</name><count>5</count></Opt>");
    const auto back = xml::deserializer{out}.load<Opt>();
    CHECK_EQ(back, Opt{"a", 5});
  }
  SUBCASE("absent -> omitted")
  {
    std::string out;
    {
      xml::serializer ser{out};
      ser.dump(Opt{"a", std::nullopt});
    }
    CHECK_EQ(out, "<Opt><name>a</name></Opt>");
    const auto back = xml::deserializer{out}.load<Opt>();
    CHECK_EQ(back, Opt{"a", std::nullopt});
  }
}

TEST_CASE("reflex::serde::xml: enum member roundtrip")
{
  std::string out;
  {
    xml::serializer ser{out};
    ser.dump(Enumed{"x", Color::Green});
  }
  CHECK_EQ(out, "<Enumed><name>x</name><color>Green</color></Enumed>");
  const auto back = xml::deserializer{out}.load<Enumed>();
  CHECK_EQ(back, Enumed{"x", Color::Green});
}

TEST_CASE("reflex::serde::xml::deserializer: reordered children")
{
  const std::string_view in =
      "<Basic><double-member>3.14</double-member><stringMember>hello</stringMember>"
      "<intMember>42</intMember></Basic>";
  const auto value = xml::deserializer{in}.load<Basic>();
  CHECK_EQ(value, Basic{42, "hello", 3.14});
}

TEST_CASE("reflex::serde::xml::deserializer: prolog, comments, attributes, unknowns")
{
  const std::string_view in =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<!-- a comment -->\n"
      "<Basic id=\"7\">\n"
      "  <intMember>42</intMember>\n"
      "  <!-- inline -->\n"
      "  <extra><nested>ignored</nested></extra>\n"
      "  <stringMember>hello</stringMember>\n"
      "  <double-member>3.14</double-member>\n"
      "  <selfclosed/>\n"
      "</Basic>";
  const auto value = xml::deserializer{in}.load<Basic>();
  CHECK_EQ(value, Basic{42, "hello", 3.14});
}

TEST_CASE("reflex::serde::xml: root name ignored on load")
{
  const std::string_view in =
      "<Anything><intMember>1</intMember><stringMember>x</stringMember>"
      "<double-member>2</double-member></Anything>";
  const auto value = xml::deserializer{in}.load<Basic>();
  CHECK_EQ(value, Basic{1, "x", 2.0});
}

TEST_CASE("reflex::serde::xml: bare '&' in text preserved")
{
  const std::string_view in =
      "<Basic><intMember>7</intMember><stringMember>Tom & Jerry</stringMember>"
      "<double-member>1</double-member></Basic>";
  const auto value = xml::deserializer{in}.load<Basic>();
  CHECK_EQ(value, Basic{7, "Tom & Jerry", 1.0});
}

TEST_CASE("reflex::serde::xml: named and numeric entities unescape")
{
  const std::string_view in =
      "<Basic><intMember>1</intMember>"
      "<stringMember>&lt;a&gt; &amp; &#65;&#x42;</stringMember>"
      "<double-member>2</double-member></Basic>";
  const auto value = xml::deserializer{in}.load<Basic>();
  CHECK_EQ(value.string_member, "<a> & AB");
}

TEST_CASE("reflex::serde::xml: comment ending in --->")
{
  const std::string_view in =
      "<!-- c ---><Basic><intMember>1</intMember><stringMember>x</stringMember>"
      "<double-member>2</double-member></Basic>";
  const auto value = xml::deserializer{in}.load<Basic>();
  CHECK_EQ(value, Basic{1, "x", 2.0});
}

TEST_CASE("reflex::serde::xml: whitespace-padded scalars trimmed")
{
  const std::string_view in =
      "<Basic>\n"
      "  <intMember> 42 </intMember>\n"
      "  <stringMember>hello</stringMember>\n"
      "  <double-member> 3.14 </double-member>\n"
      "</Basic>";
  const auto value = xml::deserializer{in}.load<Basic>();
  CHECK_EQ(value, Basic{42, "hello", 3.14});
}

TEST_CASE("reflex::serde::xml: empty and self-closing optional -> nullopt")
{
  SUBCASE("self-closing")
  {
    const std::string_view in = "<Opt><name>a</name><count/></Opt>";
    const auto             value = xml::deserializer{in}.load<Opt>();
    CHECK_EQ(value, Opt{"a", std::nullopt});
  }
  SUBCASE("empty element")
  {
    const std::string_view in = "<Opt><name>a</name><count></count></Opt>";
    const auto             value = xml::deserializer{in}.load<Opt>();
    CHECK_EQ(value, Opt{"a", std::nullopt});
  }
}

TEST_CASE("reflex::serde::xml: CDATA in skipped subtree does not corrupt parse")
{
  const std::string_view in =
      "<Basic><intMember>1</intMember>"
      "<extra><![CDATA[a > b < c]]></extra>"
      "<stringMember>x</stringMember><double-member>2</double-member></Basic>";
  const auto value = xml::deserializer{in}.load<Basic>();
  CHECK_EQ(value, Basic{1, "x", 2.0});
}

TEST_CASE("reflex::serde::xml: file roundtrip")
{
  const std::filesystem::path xml_path = "test.xml";
  const Basic                 expected{42, "hello", 3.14};
  {
    std::ofstream   out_file{xml_path};
    xml::serializer ser{out_file};
    ser.dump(expected);
  }
  {
    std::ifstream in_file{xml_path};
    const auto    value = xml::deserializer{in_file}.load<Basic>();
    CHECK_EQ(value, expected);
  }
  std::filesystem::remove(xml_path);
}

// A user-defined tag_invoke on tag_t<serialize>/tag_t<deserialize> beats the
// generic default-layer aggregate implementation (two-layer CPO dispatch).
struct user_type
{
  int         a;
  std::string b;

  constexpr bool operator==(user_type const& other) const = default;
};

template <typename OutputIt>
auto tag_invoke(
    tag_t<serde::serialize>,
    serde::xml::serializer<OutputIt>& ser,
    [[maybe_unused]] user_type const& value) -> OutputIt
{
  return std::format_to(ser.out(), "<user/>");
}

template <typename InputIt>
auto tag_invoke(
    tag_t<serde::deserialize>,
    serde::xml::deserializer<InputIt>& de,
    std::type_identity<user_type>)
{
  de.read_open_tag();
  return user_type{42, "Hello, world!"};
}

TEST_CASE("reflex::serde::xml: user-defined override")
{
  const user_type expected{42, "Hello, world!"};
  std::string     out;
  {
    xml::serializer ser{out};
    ser.dump(expected);
  }
  CHECK_EQ(out, "<user/>"sv);
  const auto value = xml::deserializer{out}.load<user_type>();
  CHECK_EQ(value, expected);
}

struct Wrapper
{
  user_type u;
};

TEST_CASE("reflex::serde::xml: nested user-defined override ignored below root")
{
  // User tag_invoke overrides apply only at the document root, not to nested
  // members: XML tag naming is member-driven.
  std::string out;
  {
    xml::serializer ser{out};
    ser.dump(Wrapper{{1, "x"}});
  }
  CHECK_EQ(out, "<Wrapper><u><a>1</a><b>x</b></u></Wrapper>");
}

struct Nested
{
  std::vector<std::vector<int>> matrix;
};
struct NonCharArray
{
  std::array<int, 3> xs;
};
struct CharArray
{
  std::array<char, 8> tag;
};
static_assert(not xml::xml_element_c<Nested>);
static_assert(not xml::xml_element_c<NonCharArray>); // only char arrays are text
static_assert(xml::xml_element_c<CharArray>);
static_assert(xml::xml_element_c<Basic>);
static_assert(xml::xml_element_c<Outer>);
static_assert(xml::xml_element_c<WithSeq>);
