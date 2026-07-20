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
  // Name-aware: honor the pending member name so it roundtrips at any depth.
  return std::format_to(ser.out(), "<{}/>", ser.element_name("user"));
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

  constexpr bool operator==(Wrapper const& other) const = default;
};

TEST_CASE("reflex::serde::xml: user-defined override honored below root")
{
  // Routing members through the CPO honors the override at any depth; being
  // name-aware, it emits the member tag <u/> and roundtrips.
  std::string out;
  {
    xml::serializer ser{out};
    ser.dump(Wrapper{{1, "x"}});
  }
  CHECK_EQ(out, "<Wrapper><u/></Wrapper>");
  const auto value = xml::deserializer{out}.load<Wrapper>();
  CHECK_EQ(value, Wrapper{{42, "Hello, world!"}});
}

// A user override that names its element via ser.element_name stays correct
// under a member name too, and roundtrips through the stashed open tag.
struct Named
{
  int v;

  constexpr bool operator==(Named const& other) const = default;
};

template <typename OutputIt>
auto tag_invoke(tag_t<serde::serialize>, serde::xml::serializer<OutputIt>& ser, Named const& value)
    -> OutputIt
{
  const auto name = ser.element_name("Named");
  return std::format_to(ser.out(), "<{0}>{1}</{0}>", name, value.v);
}

template <typename InputIt>
auto tag_invoke(
    tag_t<serde::deserialize>,
    serde::xml::deserializer<InputIt>& de,
    std::type_identity<Named>)
{
  auto [name, self_closing] = de.read_open_tag();
  const int v               = std::stoi(de.read_text());
  de.read_close_tag();
  return Named{v};
}

struct HasNamed
{
  [[= serde::rename{"item"}]] Named n;

  constexpr bool operator==(HasNamed const& other) const = default;
};

TEST_CASE("reflex::serde::xml: name-aware override uses member tag")
{
  std::string out;
  {
    xml::serializer ser{out};
    ser.dump(HasNamed{{7}});
  }
  CHECK_EQ(out, "<HasNamed><item>7</item></HasNamed>");
  const auto value = xml::deserializer{out}.load<HasNamed>();
  CHECK_EQ(value, HasNamed{{7}});
}

struct[[= derive(Debug)]] Price
{
  [[= xml::attribute]] std::string currency;
  double                           amount;

  constexpr bool operator==(Price const& other) const = default;
};

struct[[= serde::naming::camel_case, = derive(Debug)]] AttrOpt
{
  [[= xml::attribute]] std::optional<std::string> lang;
  [[= xml::attribute, = serde::rename{"ver"}]] int version;
  std::string                                     body;

  constexpr bool operator==(AttrOpt const& other) const = default;
};

TEST_CASE("reflex::serde::xml: attribute member roundtrip")
{
  std::string out;
  {
    xml::serializer ser{out};
    ser.dump(Price{"USD", 42.5});
  }
  CHECK_EQ(out, "<Price currency=\"USD\"><amount>42.5</amount></Price>");
  const auto value = xml::deserializer{out}.load<Price>();
  CHECK_EQ(value, Price{"USD", 42.5});
}

TEST_CASE("reflex::serde::xml: attribute escaping and entities")
{
  const Price value{"a & \"b\" < c", 1.0};
  std::string out;
  {
    xml::serializer ser{out};
    ser.dump(value);
  }
  CHECK(out.find("currency=\"a &amp; &quot;b&quot; &lt; c\"") != std::string::npos);
  const auto back = xml::deserializer{out}.load<Price>();
  CHECK_EQ(back, value);
}

TEST_CASE("reflex::serde::xml: optional attribute + rename")
{
  SUBCASE("present")
  {
    std::string out;
    {
      xml::serializer ser{out};
      ser.dump(AttrOpt{"en", 3, "hi"});
    }
    CHECK_EQ(out, "<AttrOpt lang=\"en\" ver=\"3\"><body>hi</body></AttrOpt>");
    const auto value = xml::deserializer{out}.load<AttrOpt>();
    CHECK_EQ(value, AttrOpt{"en", 3, "hi"});
  }
  SUBCASE("absent optional attribute omitted")
  {
    std::string out;
    {
      xml::serializer ser{out};
      ser.dump(AttrOpt{std::nullopt, 3, "hi"});
    }
    CHECK_EQ(out, "<AttrOpt ver=\"3\"><body>hi</body></AttrOpt>");
    const auto value = xml::deserializer{out}.load<AttrOpt>();
    CHECK_EQ(value, AttrOpt{std::nullopt, 3, "hi"});
  }
}

TEST_CASE("reflex::serde::xml: attribute read tolerance")
{
  // single quotes, unknown attribute, reordered, entity in value
  const std::string_view in =
      "<Price unknown='x' currency='m&amp;m'><amount>2</amount></Price>";
  const auto value = xml::deserializer{in}.load<Price>();
  CHECK_EQ(value, Price{"m&m", 2.0});
}

struct[[= derive(Debug)]] Measure
{
  [[= xml::attribute]] std::string unit;
  [[= xml::text]] double           value;

  constexpr bool operator==(Measure const& other) const = default;
};

struct[[= derive(Debug)]] OptText
{
  [[= xml::attribute]] std::string  id;
  [[= xml::text]] std::optional<int> value;

  constexpr bool operator==(OptText const& other) const = default;
};

struct[[= derive(Debug)]] Doc
{
  [[= xml::attribute]] std::string lang;
  [[= xml::raw_content]] std::string body;

  constexpr bool operator==(Doc const& other) const = default;
};

TEST_CASE("reflex::serde::xml: text member roundtrip")
{
  std::string out;
  {
    xml::serializer ser{out};
    ser.dump(Measure{"kg", 42.5});
  }
  CHECK_EQ(out, "<Measure unit=\"kg\">42.5</Measure>");
  const auto value = xml::deserializer{out}.load<Measure>();
  CHECK_EQ(value, Measure{"kg", 42.5});
}

TEST_CASE("reflex::serde::xml: optional text member absent -> self-closing")
{
  SUBCASE("present")
  {
    std::string out;
    {
      xml::serializer ser{out};
      ser.dump(OptText{"a", 7});
    }
    CHECK_EQ(out, "<OptText id=\"a\">7</OptText>");
    const auto value = xml::deserializer{out}.load<OptText>();
    CHECK_EQ(value, OptText{"a", 7});
  }
  SUBCASE("absent")
  {
    std::string out;
    {
      xml::serializer ser{out};
      ser.dump(OptText{"a", std::nullopt});
    }
    CHECK_EQ(out, "<OptText id=\"a\"/>");
    const auto value = xml::deserializer{out}.load<OptText>();
    CHECK_EQ(value, OptText{"a", std::nullopt});
  }
}

TEST_CASE("reflex::serde::xml: text member escaping")
{
  const Measure value{"a & b", 1.0};
  std::string   out;
  {
    xml::serializer ser{out};
    ser.dump(value);
  }
  // 1.0 -> "1", unit escaped in attribute
  CHECK(out.find("unit=\"a &amp; b\"") != std::string::npos);
}

TEST_CASE("reflex::serde::xml: raw_content roundtrip byte-exact")
{
  const Doc value{"en", "<p>hi</p>text<b/><![CDATA[keep]]>"};
  std::string out;
  {
    xml::serializer ser{out};
    ser.dump(value);
  }
  CHECK_EQ(out, "<Doc lang=\"en\"><p>hi</p>text<b/><![CDATA[keep]]></Doc>");
  const auto back = xml::deserializer{out}.load<Doc>();
  CHECK_EQ(back, value);
}

TEST_CASE("reflex::serde::xml: raw_content nested depth")
{
  const std::string_view in =
      "<Doc lang=\"x\"><a><a>deep</a></a>tail</Doc>";
  const auto value = xml::deserializer{in}.load<Doc>();
  CHECK_EQ(value.lang, "x");
  CHECK_EQ(value.body, "<a><a>deep</a></a>tail");
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
struct AggAttr
{
  [[= xml::attribute]] Inner bad; // aggregate attribute is invalid
};
struct SeqAttr
{
  [[= xml::attribute]] std::vector<int> bad; // sequence attribute is invalid
};
static_assert(not xml::xml_element_c<Nested>);
static_assert(not xml::xml_element_c<NonCharArray>); // only char arrays are text
static_assert(xml::xml_element_c<CharArray>);
static_assert(xml::xml_element_c<Basic>);
static_assert(xml::xml_element_c<Outer>);
static_assert(xml::xml_element_c<WithSeq>);
static_assert(xml::xml_element_c<Price>);
static_assert(not xml::xml_element_c<AggAttr>); // attribute must be scalar
static_assert(not xml::xml_element_c<SeqAttr>);

struct TwoText
{
  [[= xml::text]] int a;
  [[= xml::text]] int b;
};
struct TextPlusChild
{
  [[= xml::text]] int  a;
  std::string          child;
};
struct RawPlusText
{
  [[= xml::text]] int         a;
  [[= xml::raw_content]] std::string body;
};
static_assert(xml::xml_element_c<Measure>);
static_assert(xml::xml_element_c<Doc>);
static_assert(not xml::xml_element_c<TwoText>);       // at most one text member
static_assert(not xml::xml_element_c<TextPlusChild>); // text excludes child elements
static_assert(not xml::xml_element_c<RawPlusText>);   // text and raw are exclusive
