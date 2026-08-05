#include <doctest/doctest.h>

import reflex.serde.xml;
import serde.tests.types;

import std;

using namespace reflex;
using namespace reflex::serde;
using namespace std::literals;

// The bulk-scan cliff, pinned. An in-memory input parses with memchr-backed
// scans, a stream cursor falls back to one character at a time. Both answers
// are correct, and the point of bulk_scan being public is that a caller can
// find out at compile time which one they got.
static_assert(xml::deserializer<std::string_view::const_iterator>::bulk_scan);
static_assert(not xml::deserializer<std::istreambuf_iterator<char>>::bulk_scan);

struct[[= serde::naming::camel_case, = derive(Debug)]] Basic
{
  int                                    int_member;
  std::string                            string_member;
  [[= serde::naming::kebab_case]] double double_member;

  constexpr bool operator==(Basic const& other) const = default;
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

struct[[= derive(Debug)]] Script
{
  std::string             name;
  [[= xml::cdata]] std::string code;

  constexpr bool operator==(Script const& other) const = default;
};

TEST_CASE("reflex::serde::xml: cdata child element write + read")
{
  const Script value{"s", "if (a < b && c > d) x;"};
  std::string  out;
  {
    xml::serializer ser{out};
    ser.dump(value);
  }
  CHECK_EQ(
      out,
      "<Script><name>s</name>"
      "<code><![CDATA[if (a < b && c > d) x;]]></code></Script>");
  const auto back = xml::deserializer{out}.load<Script>();
  CHECK_EQ(back, value);
}

TEST_CASE("reflex::serde::xml: cdata payload containing ]]> is split")
{
  const Script value{"s", "a]]>b"};
  std::string  out;
  {
    xml::serializer ser{out};
    ser.dump(value);
  }
  CHECK_EQ(
      out,
      "<Script><name>s</name>"
      "<code><![CDATA[a]]]]><![CDATA[>b]]></code></Script>");
  const auto back = xml::deserializer{out}.load<Script>();
  CHECK_EQ(back, value); // two CDATA sections concatenate back to "a]]>b"
}

TEST_CASE("reflex::serde::xml: read CDATA in text content")
{
  SUBCASE("cdata only")
  {
    const std::string_view in = "<Basic><intMember>1</intMember>"
                                "<stringMember><![CDATA[a>b&c<d]]></stringMember>"
                                "<double-member>2</double-member></Basic>";
    const auto value = xml::deserializer{in}.load<Basic>();
    CHECK_EQ(value.string_member, "a>b&c<d");
  }
  SUBCASE("text + cdata + text concatenate")
  {
    const std::string_view in = "<Basic><intMember>1</intMember>"
                                "<stringMember>x<![CDATA[ & ]]>y</stringMember>"
                                "<double-member>2</double-member></Basic>";
    const auto value = xml::deserializer{in}.load<Basic>();
    CHECK_EQ(value.string_member, "x & y");
  }
}

TEST_CASE("reflex::serde::xml: CDATA in skipped unknown subtree")
{
  const std::string_view in =
      "<Basic><intMember>1</intMember>"
      "<extra><![CDATA[a > b ]]></extra>"
      "<stringMember>x</stringMember><double-member>2</double-member></Basic>";
  const auto value = xml::deserializer{in}.load<Basic>();
  CHECK_EQ(value, Basic{1, "x", 2.0});
}

struct[[= xml::ns{"x", "urn:example:e"}, = derive(Debug)]] NsRoot
{
  int         a;
  std::string b;

  constexpr bool operator==(NsRoot const& other) const = default;
};

struct[[= derive(Debug)]] Coll
{
  [[= serde::rename{"x:val"}]] int qualified;
  int                              val;

  constexpr bool operator==(Coll const& other) const = default;
};

TEST_CASE("reflex::serde::xml: namespace write + roundtrip")
{
  const NsRoot value{1, "hi"};
  std::string  out;
  {
    xml::serializer ser{out};
    ser.dump(value);
  }
  CHECK_EQ(
      out,
      "<x:NsRoot xmlns:x=\"urn:example:e\">"
      "<x:a>1</x:a><x:b>hi</x:b></x:NsRoot>");
  const auto back = xml::deserializer{out}.load<NsRoot>();
  CHECK_EQ(back, value);
}

TEST_CASE("reflex::serde::xml: prefixed document loads into plain type")
{
  const std::string_view in =
      "<n:Basic xmlns:n=\"urn:x\"><n:intMember>42</n:intMember>"
      "<n:stringMember>x</n:stringMember><n:double-member>3.14</n:double-member></n:Basic>";
  const auto value = xml::deserializer{in}.load<Basic>();
  CHECK_EQ(value, Basic{42, "x", 3.14});
}

TEST_CASE("reflex::serde::xml: qualified match wins over local")
{
  // <x:val> exactly matches the renamed member; the plain 'val' stays default
  const std::string_view in = "<Coll><x:val>7</x:val></Coll>";
  const auto             value = xml::deserializer{in}.load<Coll>();
  CHECK_EQ(value, Coll{7, 0});
}

struct[[= derive(Debug)]] Range
{
  [[= xml::attribute]] double min;
  [[= xml::attribute]] double max;

  constexpr bool operator==(Range const& other) const = default;
};

struct[[= derive(Debug)]] Limits
{
  std::string          name;
  std::optional<Range> voltage;

  constexpr bool operator==(Limits const& other) const = default;
};

TEST_CASE("reflex::serde::xml: optional aggregate carried by a self-closing element")
{
  // A self-closing element has no body but its attributes still hold the value,
  // so an optional aggregate must not be discarded as absent.
  SUBCASE("self-closing with attributes")
  {
    const std::string_view in =
        "<Limits><name>u5</name><voltage min=\"1.71\" max=\"3.6\"/></Limits>";
    const auto value = xml::deserializer{in}.load<Limits>();
    REQUIRE(value.voltage.has_value());
    CHECK_EQ(value.voltage->min, doctest::Approx(1.71));
    CHECK_EQ(value.voltage->max, doctest::Approx(3.6));
  }
  SUBCASE("element absent -> nullopt")
  {
    const std::string_view in    = "<Limits><name>u5</name></Limits>";
    const auto             value = xml::deserializer{in}.load<Limits>();
    CHECK_FALSE(value.voltage.has_value());
  }
  SUBCASE("roundtrip")
  {
    const Limits expected{"u5", Range{1.71, 3.6}};
    std::string  out;
    {
      xml::serializer ser{out};
      ser.dump(expected);
    }
    CHECK_EQ(
        out,
        "<Limits><name>u5</name>"
        "<voltage min=\"1.71\" max=\"3.6\"></voltage></Limits>");
    const auto back = xml::deserializer{out}.load<Limits>();
    CHECK_EQ(back, expected);
  }
}

TEST_CASE("reflex::serde::xml: self-closing optional text member stays nullopt")
{
  // the text counterpart: no body means no text, so still absent
  const std::string_view in    = "<Opt><name>a</name><count/></Opt>";
  const auto             value = xml::deserializer{in}.load<Opt>();
  CHECK_EQ(value, Opt{"a", std::nullopt});
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

// An enum deriving Format is written through its own formatter, by name, so it
// has to be read back by name. A plain enum is written as its underlying
// integer. The two live side by side here so the representations stay
// distinguishable: if the parse side ever keys on the wrong predicate, one of
// these two round-trips breaks.
enum class PlainEnum
{
  red  = 0,
  blue = 7,
  neg  = -3,
};

enum class[[= derive(Format)]] NamedEnum
{
  alpha = 1,
  beta  = 2,
};

enum class[[= derive(Format, EnumFlags)]] FlagEnum
{
  none = 0,
  read = 1,
  write = 2,
  exec = 4,
};

struct[[= derive(Debug)]] EnumText
{
  PlainEnum plain;
  NamedEnum named;

  constexpr bool operator==(EnumText const& other) const = default;
};

struct[[= derive(Debug)]] EnumAttrs
{
  [[= xml::attribute]] PlainEnum plain;
  [[= xml::attribute]] NamedEnum named;
  int                            filler;

  constexpr bool operator==(EnumAttrs const& other) const = default;
};

struct[[= derive(Debug)]] FlagText
{
  FlagEnum flags;

  constexpr bool operator==(FlagText const& other) const = default;
};

TEST_CASE("reflex::serde::xml: Format-derived enum round-trips by name")
{
  SUBCASE("element text")
  {
    const EnumText value{PlainEnum::blue, NamedEnum::beta};
    std::string    out;
    {
      xml::serializer ser{out};
      ser.dump(value);
    }
    // the plain enum keeps its integer spelling, the Format-derived one its name
    CHECK_EQ(out, "<EnumText><plain>7</plain><named>beta</named></EnumText>");
    CHECK_EQ(xml::deserializer{out}.load<EnumText>(), value);
  }

  SUBCASE("attributes")
  {
    const EnumAttrs value{PlainEnum::neg, NamedEnum::alpha, 5};
    std::string     out;
    {
      xml::serializer ser{out};
      ser.dump(value);
    }
    CHECK_EQ(out, R"(<EnumAttrs plain="-3" named="alpha"><filler>5</filler></EnumAttrs>)");
    CHECK_EQ(xml::deserializer{out}.load<EnumAttrs>(), value);
  }

  SUBCASE("every enumerator")
  {
    for(const auto e : {NamedEnum::alpha, NamedEnum::beta})
    {
      const EnumText value{PlainEnum::red, e};
      std::string    out;
      {
        xml::serializer ser{out};
        ser.dump(value);
      }
      CHECK_EQ(xml::deserializer{out}.load<EnumText>(), value);
    }
  }

  SUBCASE("a flags enum round-trips its composite spelling")
  {
    using namespace reflex::bitwise_operations;
    const FlagText value{FlagEnum::read | FlagEnum::exec};
    std::string    out;
    {
      xml::serializer ser{out};
      ser.dump(value);
    }
    CHECK_EQ(out, "<FlagText><flags>read|exec</flags></FlagText>");
    CHECK_EQ(xml::deserializer{out}.load<FlagText>(), value);
  }

  SUBCASE("an integer body still parses for both kinds")
  {
    // documents written by hand, and anything predating the name spelling
    CHECK_EQ(xml::deserializer{"<EnumText><plain>7</plain><named>2</named></EnumText>"s}
                 .load<EnumText>(),
             EnumText{PlainEnum::blue, NamedEnum::beta});
  }

  SUBCASE("a plain enum still rejects a name")
  {
    // only the Format-derived side gained the name spelling
    CHECK_THROWS(
        xml::deserializer{"<EnumText><plain>blue</plain><named>beta</named></EnumText>"s}
            .load<EnumText>());
  }
}

// bulk_scan is the parser's fast-path predicate. It is public so a caller can
// assert at compile time which path they are on, rather than discovering the
// difference as an unexplained slowdown.
static_assert(xml::deserializer<std::string_view::const_iterator>::bulk_scan);
static_assert(xml::deserializer<std::string::const_iterator>::bulk_scan);
static_assert(not xml::deserializer<std::istreambuf_iterator<char>>::bulk_scan);

// A tag name is borrowed from the input when the input is in memory. Pinned so
// a refactor cannot quietly materialize it again.
static_assert(
    std::same_as<
        xml::deserializer<std::string_view::const_iterator>::name_t,
        std::string_view>);
static_assert(
    std::same_as<xml::deserializer<std::istreambuf_iterator<char>>::name_t, std::string>);

// Attribute names and values follow the same rule, so a caller has one lifetime
// to reason about instead of three.
static_assert(
    std::same_as<
        xml::deserializer<std::string_view::const_iterator>::attr_str,
        std::string_view>);
static_assert(
    std::same_as<xml::deserializer<std::istreambuf_iterator<char>>::attr_str, std::string>);

// A deserialize override that keeps the borrowed tag name instead of copying
// it. That is legal exactly while the input buffer is alive, which is what
// holding an mmap_input_stream guarantees.
inline std::string_view borrowed_tag{};

struct Borrows
{
  int v;

  constexpr bool operator==(Borrows const& other) const = default;
};

template <typename OutputIt>
auto tag_invoke(tag_t<serde::serialize>, serde::xml::serializer<OutputIt>& ser, Borrows const& value)
    -> OutputIt
{
  const auto name = ser.element_name("Borrows");
  return std::format_to(ser.out(), "<{0}>{1}</{0}>", name, value.v);
}

template <typename InputIt>
auto tag_invoke(
    tag_t<serde::deserialize>,
    serde::xml::deserializer<InputIt>& de,
    std::type_identity<Borrows>)
{
  auto [name, self_closing] = de.read_open_tag();
  if constexpr(serde::xml::deserializer<InputIt>::bulk_scan)
  {
    borrowed_tag = name; // a view into the caller's buffer, not a copy
  }
  const int v = std::stoi(de.read_text());
  de.read_close_tag();
  return Borrows{v};
}

TEST_CASE("reflex::serde::xml: deserializing from an mmap_input_stream")
{
  const std::filesystem::path xml_path = "test-mmap-input-stream.xml";

  SUBCASE("reads a whole document through the usual deserializer API")
  {
    const WithSeq expected{"xs", {1, 2, 3}};
    {
      std::ofstream   out_file{xml_path};
      xml::serializer ser{out_file};
      ser.dump(expected);
    }
    serde::mmap_input_stream in{xml_path};
    auto                     de = xml::deserializer{in};
    static_assert(decltype(de)::bulk_scan);
    CHECK_EQ(de.load<WithSeq>(), expected);
    std::filesystem::remove(xml_path);
  }

  SUBCASE("the result owns its strings")
  {
    const Basic expected{7, "a value comfortably past the small string limit", 1.5};
    {
      std::ofstream   out_file{xml_path};
      xml::serializer ser{out_file};
      ser.dump(expected);
    }
    Basic value{};
    {
      serde::mmap_input_stream in{xml_path};
      value = xml::deserializer{in}.load<Basic>();
    } // the mapping is gone here, the strings are not
    CHECK_EQ(value.string_member, expected.string_member);
    CHECK_EQ(value, expected);
    std::filesystem::remove(xml_path);
  }

  SUBCASE("a borrowed tag name stays valid while the stream lives")
  {
    {
      std::ofstream out_file{xml_path, std::ios::binary};
      out_file << "<Borrows>7</Borrows>";
    }
    borrowed_tag = {};
    serde::mmap_input_stream in{xml_path};
    CHECK_EQ(xml::deserializer{in}.load<Borrows>(), Borrows{7});
    // the deserializer is gone, `in` is not, so reading the view is defined
    CHECK_EQ(borrowed_tag, "Borrows"sv);
    CHECK(borrowed_tag.data() >= in.begin());
    CHECK(borrowed_tag.data() + borrowed_tag.size() <= in.end());
    borrowed_tag = {};
    std::filesystem::remove(xml_path);
  }

  SUBCASE("entities and CDATA are decoded, not aliased")
  {
    {
      std::ofstream out_file{xml_path};
      out_file << "<WithSeq><name>a &amp; b<![CDATA[ &raw ]]></name>"
                  "<values>1</values></WithSeq>";
    }
    serde::mmap_input_stream in{xml_path};
    CHECK_EQ(xml::deserializer{in}.load<WithSeq>().name, "a & b &raw ");
    std::filesystem::remove(xml_path);
  }

  SUBCASE("an empty file is not a document")
  {
    {
      std::ofstream out_file{xml_path};
    }
    serde::mmap_input_stream in{xml_path};
    CHECK(in.empty());
    CHECK_THROWS(xml::deserializer{in}.load<Basic>());
    std::filesystem::remove(xml_path);
  }

  SUBCASE("a truncated document throws rather than reading off the end")
  {
    {
      std::ofstream out_file{xml_path, std::ios::binary};
      out_file << "<WithSeq><name>abc";
    }
    serde::mmap_input_stream in{xml_path};
    CHECK_THROWS(xml::deserializer{in}.load<WithSeq>());
    std::filesystem::remove(xml_path);
  }

  SUBCASE("no trailing newline is fine")
  {
    {
      std::ofstream out_file{xml_path, std::ios::binary};
      out_file << "<WithSeq><name>tail</name><values>9</values></WithSeq>";
    }
    serde::mmap_input_stream in{xml_path};
    CHECK_EQ(xml::deserializer{in}.load<WithSeq>(), WithSeq{"tail", {9}});
    std::filesystem::remove(xml_path);
  }
}

namespace
{
  bool aliases(std::string_view part, std::string_view whole)
  {
    return part.data() >= whole.data()
       and part.data() + part.size() <= whole.data() + whole.size();
  }

  // Reads through the non-contiguous cursor, where nothing borrows. Every
  // attribute must come back the same as on the contiguous path.
  template <typename T> T load_streaming(std::string_view text)
  {
    std::istringstream in{std::string{text}};
    return xml::deserializer{in}.template load<T>();
  }
} // namespace

struct[[= derive(Debug)]] AttrText
{
  [[= xml::attribute]] std::string a;

  constexpr bool operator==(AttrText const& other) const = default;
};

TEST_CASE("reflex::serde::xml::deserializer: attributes borrow from the input")
{
  const std::string_view in = R"(<e plain="v" enc="a&amp;b" empty="" sq='s'/>)";
  xml::deserializer      de{in};
  de.read_open_tag();
  auto const& attrs = de.attributes();
  REQUIRE_EQ(attrs.size(), 4u);

  // a name can never hold an entity, so it is always a span of the input
  for(auto const& [name, value] : attrs)
  {
    CHECK(aliases(name, in));
  }

  CHECK_EQ(attrs[0].first, "plain"sv);
  CHECK_EQ(attrs[0].second, "v"sv);
  CHECK(aliases(attrs[0].second, in));

  // a value that had to be decoded is materialized, so it cannot alias the
  // input, but it must still read correctly
  CHECK_EQ(attrs[1].second, "a&b"sv);
  CHECK_FALSE(aliases(attrs[1].second, in));

  CHECK_EQ(attrs[2].second, ""sv);
  CHECK_EQ(attrs[3].second, "s"sv);
  CHECK(aliases(attrs[3].second, in));
}

TEST_CASE("reflex::serde::xml::deserializer: an attribute view outlives the next open tag")
{
  // What borrowing buys: tag names, attribute names and attribute values all
  // follow one lifetime rule instead of three, so a view taken from one element
  // still reads after the parse has moved on to the next.
  const std::string in = R"(<root a="one" e="x&amp;y"><child b="two"/></root>)";
  std::string_view  kept_name{}, kept_value{};
  {
    xml::deserializer de{std::string_view{in}};
    de.read_open_tag();
    kept_name                           = de.attributes()[0].first;
    kept_value                          = de.attributes()[0].second;
    const std::string_view kept_decoded = de.attributes()[1].second;

    de.read_open_tag(); // the list is rebuilt for <child>
    REQUIRE_EQ(de.attributes().size(), 1u);
    CHECK_EQ(de.attributes()[0].first, "b"sv);
    CHECK_EQ(kept_name, "a"sv);
    CHECK_EQ(kept_value, "one"sv);
    // a decoded value is owned by the deserializer, so it lives this long too
    CHECK_EQ(kept_decoded, "x&y"sv);
  }
  // the deserializer is gone, the input buffer is not
  CHECK_EQ(kept_name, "a"sv);
  CHECK_EQ(kept_value, "one"sv);
}

TEST_CASE("reflex::serde::xml::deserializer: entities in attribute values")
{
  const std::pair<std::string_view, std::string_view> cases[]{
      {"&amp;", "&"},
      {"&lt;", "<"},
      {"&gt;", ">"},
      {"&quot;", "\""},
      {"&apos;", "'"},
      {"&#65;", "A"},
      {"&#x42;", "B"},
      {"a&amp;b&lt;c&gt;d", "a&b<c>d"},
      {"&amp;tail", "&tail"},
      {"head&amp;", "head&"},
      {"&amp;&amp;", "&&"},
      {"plain", "plain"},
  };
  for(auto const& [raw, decoded] : cases)
  {
    CAPTURE(raw);
    const std::string in = std::format(R"(<AttrText a="{}"/>)", raw);
    CHECK_EQ(xml::deserializer{std::string_view{in}}.load<AttrText>().a, decoded);
    CHECK_EQ(load_streaming<AttrText>(in).a, decoded);
  }
}

struct[[= derive(Debug)]] AttrPairText
{
  [[= xml::attribute]] std::string a;
  [[= xml::attribute]] std::string b;
  [[= xml::text]] std::string      text;

  constexpr bool operator==(AttrPairText const& other) const = default;
};

TEST_CASE("reflex::serde::xml::deserializer: an unterminated entity in an attribute is kept raw")
{
  // element content preserves a malformed reference verbatim and never eats the
  // markup behind it, an attribute value has to follow the same policy
  SUBCASE("element content, the behaviour the attribute path must match")
  {
    const std::string_view in =
        "<Basic><intMember>1</intMember><stringMember>&amp</stringMember>"
        "<double-member>2</double-member></Basic>";
    CHECK_EQ(xml::deserializer{in}.load<Basic>().string_member, "&amp");
    CHECK_EQ(load_streaming<Basic>(in).string_member, "&amp");
  }
  SUBCASE("double-quoted attribute")
  {
    const std::string_view in = R"(<AttrText a="&amp"/>)";
    CHECK_EQ(xml::deserializer{in}.load<AttrText>().a, "&amp");
    CHECK_EQ(load_streaming<AttrText>(in).a, "&amp");
  }
  SUBCASE("single-quoted attribute")
  {
    const std::string_view in = R"(<AttrText a='&amp'/>)";
    CHECK_EQ(xml::deserializer{in}.load<AttrText>().a, "&amp");
    CHECK_EQ(load_streaming<AttrText>(in).a, "&amp");
  }
  SUBCASE("a bare ampersand is a value of its own")
  {
    const std::string_view in = R"(<AttrText a="&"/>)";
    CHECK_EQ(xml::deserializer{in}.load<AttrText>().a, "&");
    CHECK_EQ(load_streaming<AttrText>(in).a, "&");
  }
  SUBCASE("the scan stops at the quote, the rest of the element still parses")
  {
    const std::string_view in = R"(<AttrPairText a="&amp" b="ok">tail</AttrPairText>)";
    const AttrPairText     expected{"&amp", "ok", "tail"};
    CHECK_EQ(xml::deserializer{in}.load<AttrPairText>(), expected);
    CHECK_EQ(load_streaming<AttrPairText>(in), expected);
  }
  SUBCASE("an unknown reference mid-value keeps its surroundings")
  {
    const std::string_view in = R"(<AttrPairText a="x&ampy" b="ok">tail</AttrPairText>)";
    const AttrPairText     expected{"x&ampy", "ok", "tail"};
    CHECK_EQ(xml::deserializer{in}.load<AttrPairText>(), expected);
    CHECK_EQ(load_streaming<AttrPairText>(in), expected);
  }
}

TEST_CASE("reflex::serde::xml::deserializer: attribute shapes")
{
  SUBCASE("empty value")
  {
    const std::string_view in = R"(<AttrText a=""/>)";
    CHECK_EQ(xml::deserializer{in}.load<AttrText>().a, "");
    CHECK_EQ(load_streaming<AttrText>(in).a, "");
  }
  SUBCASE("single-quoted value, holding the other quote and an entity")
  {
    const std::string_view in = R"(<AttrText a='say "it&apos;s"'/>)";
    CHECK_EQ(xml::deserializer{in}.load<AttrText>().a, "say \"it's\"");
    CHECK_EQ(load_streaming<AttrText>(in).a, "say \"it's\"");
  }
  SUBCASE("attribute-only self-closing element")
  {
    const std::string_view in = R"(<Range min="1.5" max="2.5"/>)";
    CHECK_EQ(xml::deserializer{in}.load<Range>(), Range{1.5, 2.5});
    CHECK_EQ(load_streaming<Range>(in), Range{1.5, 2.5});
  }
  SUBCASE("namespace declarations are skipped, not assigned")
  {
    const std::string_view in = R"(<n:AttrText xmlns="urn:d" xmlns:n="urn:x" n:a="v"/>)";
    CHECK_EQ(xml::deserializer{in}.load<AttrText>().a, "v");
    CHECK_EQ(load_streaming<AttrText>(in).a, "v");
  }
}

struct[[= derive(Debug)]] Cell
{
  [[= xml::attribute]] std::string name;
  [[= xml::attribute]] int         row;
  [[= xml::attribute]] int         col;
  [[= xml::attribute]] std::string kind;

  constexpr bool operator==(Cell const& other) const = default;
};

struct[[= derive(Debug)]] Sheet
{
  [[= xml::attribute]] std::string title;
  std::vector<Cell>                cell;

  constexpr bool operator==(Sheet const& other) const = default;
};

TEST_CASE("reflex::serde::xml: attribute-dense document round-trips byte for byte")
{
  Sheet expected{"a & b <sheet>", {}};
  for(int i = 0; i < 32; ++i)
  {
    expected.cell.push_back(Cell{std::format("c{}", i), i, i * 2, (i % 2) ? "num" : R"(a "&" b)"});
  }
  std::string out;
  {
    xml::serializer ser{out};
    ser.dump(expected);
  }
  CHECK_EQ(xml::deserializer{std::string_view{out}}.load<Sheet>(), expected);
  CHECK_EQ(load_streaming<Sheet>(out), expected);

  std::string again;
  {
    xml::serializer ser{again};
    ser.dump(xml::deserializer{std::string_view{out}}.load<Sheet>());
  }
  CHECK_EQ(again, out);
}

TEST_CASE("reflex::serde::xml::deserializer: a member reads only under its serialized name")
{
  // Basic is camelCase with double_member kebab-cased, so the serialized names
  // are intMember, stringMember and double-member. XML resolves elements and
  // attributes with its own matching code, separate from object_visit, so the
  // rule needs pinning here too.
  SUBCASE("the serialized name resolves")
  {
    const std::string_view in =
        "<Basic><intMember>7</intMember><stringMember>hi</stringMember>"
        "<double-member>1.5</double-member></Basic>";
    const auto value = xml::deserializer{in}.load<Basic>();
    CHECK_EQ(value.int_member, 7);
    CHECK_EQ(value.string_member, "hi");
    CHECK_EQ(value.double_member, 1.5);
  }
  SUBCASE("the C++ identifier does not")
  {
    const std::string_view in =
        "<Basic><int_member>7</int_member><string_member>hi</string_member></Basic>";
    const auto value = xml::deserializer{in}.load<Basic>();
    CHECK_EQ(value.int_member, 0);
    CHECK_EQ(value.string_member, "");
  }
}

// A std::string_view member reads a view of the input instead of a copy.
//
// The borrow is only offered where the run really is the input's. A run that had
// to be decoded, an entity or a CDATA splice, lives in a buffer of XML's own that
// dies with the deserializer, so it throws rather than handing out a view of it.
// On the streaming cursor there is no buffer at all and it is a compile error:
//
//   xml::deserializer{std::istringstream{...}}.load<BorrowedText>();
//
//   error: static assertion failed: std::basic_string_view<char> cannot be an XML
//   string destination on this cursor: a borrowed read needs a contiguous input
//   to point at, and this deserializer reads a character at a time (use
//   std::string, or deserialize from a contiguous input)
//
// Before this, a plain run borrowed correctly and a decoded one silently returned
// the freed bytes of a local.
struct BorrowedText
{
  std::string_view text;
};

struct BorrowedAttr
{
  [[= xml::attribute]] std::string_view name;
  std::string_view                      text;
};

TEST_CASE("reflex::serde::xml: a borrowed string destination")
{
  SUBCASE("a plain run points into the input")
  {
    // Held by name: the view read out of it points into it.
    const std::string in = "<BorrowedText><text>hello there</text></BorrowedText>";
    const auto        v  = xml::deserializer{std::string_view{in}}.load<BorrowedText>();
    CHECK_EQ(v.text, "hello there"sv);
    // Address, not content: comparing bytes would pass for a copy too.
    CHECK_EQ(v.text.data(), in.data() + in.find("hello there"));
  }

  SUBCASE("an entity-decoded run throws rather than dangling")
  {
    const std::string in = "<BorrowedText><text>a&amp;b</text></BorrowedText>";
    CHECK_THROWS_AS(
        (xml::deserializer{std::string_view{in}}.load<BorrowedText>()), std::runtime_error);
  }

  SUBCASE("a CDATA-spliced run throws too")
  {
    const std::string in = "<BorrowedText><text>a<![CDATA[b]]>c</text></BorrowedText>";
    CHECK_THROWS_AS(
        (xml::deserializer{std::string_view{in}}.load<BorrowedText>()), std::runtime_error);
  }

  SUBCASE("an empty element borrows nothing and is fine")
  {
    const std::string in = "<BorrowedText><text></text></BorrowedText>";
    const auto        v  = xml::deserializer{std::string_view{in}}.load<BorrowedText>();
    CHECK(v.text.empty());
  }

  SUBCASE("a self-closing element is fine too")
  {
    const std::string in = "<BorrowedText><text/></BorrowedText>";
    const auto        v  = xml::deserializer{std::string_view{in}}.load<BorrowedText>();
    CHECK(v.text.empty());
  }

  SUBCASE("a plain attribute value points into the input")
  {
    const std::string in = R"(<BorrowedAttr name="plain"><text>body</text></BorrowedAttr>)";
    const auto        v  = xml::deserializer{std::string_view{in}}.load<BorrowedAttr>();
    CHECK_EQ(v.name, "plain"sv);
    CHECK_EQ(v.name.data(), in.data() + in.find("plain"));
    CHECK_EQ(v.text.data(), in.data() + in.find("body"));
  }

  SUBCASE("a decoded attribute value throws")
  {
    const std::string in = R"(<BorrowedAttr name="a&amp;b"><text>body</text></BorrowedAttr>)";
    CHECK_THROWS_AS(
        (xml::deserializer{std::string_view{in}}.load<BorrowedAttr>()), std::runtime_error);
  }
}

TEST_CASE("reflex::serde::xml: an owning string destination is unaffected")
{
  struct Owned
  {
    std::string text;

    bool operator==(Owned const& other) const = default;
  };

  SUBCASE("a plain run still copies")
  {
    const std::string in = "<Owned><text>hello there</text></Owned>";
    const auto        v  = xml::deserializer{std::string_view{in}}.load<Owned>();
    CHECK_EQ(v.text, "hello there");
    CHECK_NE(v.text.data(), in.data() + in.find("hello there"));
  }

  SUBCASE("a decoded run still decodes")
  {
    const std::string in = "<Owned><text>a&amp;b</text></Owned>";
    CHECK_EQ(xml::deserializer{std::string_view{in}}.load<Owned>().text, "a&b");
  }

  SUBCASE("and the streaming cursor still works")
  {
    std::istringstream stream{"<Owned><text>a&amp;b</text></Owned>"};
    CHECK_EQ(xml::deserializer{stream}.load<Owned>().text, "a&b");
  }
}
