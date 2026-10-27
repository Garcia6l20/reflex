#include <doctest/doctest.h>

import reflex.serde.xml;

import std;

using namespace reflex;
using namespace reflex::serde;
using namespace std::literals;

struct xp_srv
{
  [[= xml::attribute]] int port;
  std::string              host;

  constexpr bool operator==(xp_srv const&) const = default;
};

struct xp_price
{
  [[= xml::attribute]] std::string currency;
  [[= xml::text]] double           amount;

  constexpr bool operator==(xp_price const&) const = default;
};

struct xp_empty
{
  std::optional<int>                            maybe;
  [[= serde::omit_if_empty{}]] std::vector<int> none;

  constexpr bool operator==(xp_empty const&) const = default;
};

struct xp_config
{
  int                value;
  xp_srv             srv;
  std::vector<int>   list;
  xp_price           price;
  xp_empty           empty;
  std::optional<int> absent;

  constexpr bool operator==(xp_config const&) const = default;
};

struct xp_spaced
{
  [[= xml::attribute]] std::string lang;
  [[= xml::text]] std::string      body;

  constexpr bool operator==(xp_spaced const&) const = default;
};

struct xp_raw
{
  [[= xml::raw_content]] std::string inner;

  constexpr bool operator==(xp_raw const&) const = default;
};

struct xp_bodies
{
  std::string                  plain;
  xp_spaced                    spaced;
  xp_raw                       raw;
  [[= xml::cdata]] std::string script;

  constexpr bool operator==(xp_bodies const&) const = default;
};

struct[[= xml::ns{"x", "http://e"}]] xp_env
{
  int    id;
  xp_srv srv;

  constexpr bool operator==(xp_env const&) const = default;
};

struct xp_user
{
  int v;

  constexpr bool operator==(xp_user const&) const = default;
};

template <typename OutputIt, auto Opts>
auto tag_invoke(
    tag_t<serde::serialize>,
    serde::xml::serializer<OutputIt, Opts>& ser,
    xp_user const&                          value) -> OutputIt
{
  return std::format_to(ser.out(), "<{0} v=\"{1}\"/>", ser.element_name("xp_user"), value.v);
}

template <typename InputIt, auto Opts>
auto tag_invoke(
    tag_t<serde::deserialize>,
    serde::xml::deserializer<InputIt, Opts>& de,
    std::type_identity<xp_user>)
{
  de.read_open_tag();
  return xp_user{std::stoi(std::string{de.attributes().front().second})};
}

struct xp_with_user
{
  int     a;
  xp_user u;
  int     b;

  constexpr bool operator==(xp_with_user const&) const = default;
};

namespace
{
template <int Indent, typename T> std::string pretty(T const& value)
{
  std::string     out;
  xml::serializer ser{out, std::cw<xml::options{.indent = Indent}>};
  ser.dump(value);
  return out;
}

template <typename T> T read_back(std::string const& in)
{
  return xml::deserializer{in}.load<T>();
}

xp_config sample_config()
{
  return {
      1, {8080, "a"},
       {1, 2},
       {"USD", 42.5},
       {},
       std::nullopt
  };
}
} // namespace

static_assert(
    sizeof(xml::serializer<std::back_insert_iterator<std::string>>)
    == sizeof(serde::detail::serializer_base<std::back_insert_iterator<std::string>>)
    + sizeof(std::string_view));

TEST_CASE("reflex::serde::xml pretty: nested aggregates at indent 2")
{
  const std::string out = pretty<2>(sample_config());
  CHECK_EQ(
      out, "<xp_config>\n"
           "  <value>1</value>\n"
           "  <srv port=\"8080\">\n"
           "    <host>a</host>\n"
           "  </srv>\n"
           "  <list>1</list>\n"
           "  <list>2</list>\n"
           "  <price currency=\"USD\">42.5</price>\n"
           "  <empty></empty>\n"
           "</xp_config>"sv);
  CHECK_EQ(read_back<xp_config>(out), sample_config());
}

TEST_CASE("reflex::serde::xml pretty: nested aggregates at indent 4")
{
  const std::string out = pretty<4>(sample_config());
  CHECK_EQ(
      out, "<xp_config>\n"
           "    <value>1</value>\n"
           "    <srv port=\"8080\">\n"
           "        <host>a</host>\n"
           "    </srv>\n"
           "    <list>1</list>\n"
           "    <list>2</list>\n"
           "    <price currency=\"USD\">42.5</price>\n"
           "    <empty></empty>\n"
           "</xp_config>"sv);
  CHECK_EQ(read_back<xp_config>(out), sample_config());
}

TEST_CASE("reflex::serde::xml pretty: compact output is unchanged")
{
  std::string out;
  xml::serializer{out}.dump(sample_config());
  CHECK_EQ(
      out, "<xp_config><value>1</value><srv port=\"8080\"><host>a</host></srv><list>1</list>"
           "<list>2</list><price currency=\"USD\">42.5</price><empty></empty></xp_config>"sv);
}

TEST_CASE("reflex::serde::xml pretty: no whitespace inside text, raw content or cdata")
{
  const xp_bodies value{
      "  x  ", {"en", "  spaced out  "},
       {"\n  <a>1</a>\n  <b/>\n"},
       "if(a<b){}"
  };
  const std::string out = pretty<2>(value);
  CHECK_EQ(
      out, "<xp_bodies>\n"
           "  <plain>  x  </plain>\n"
           "  <spaced lang=\"en\">  spaced out  </spaced>\n"
           "  <raw>\n  <a>1</a>\n  <b/>\n</raw>\n"
           "  <script><![CDATA[if(a<b){}]]></script>\n"
           "</xp_bodies>"sv);
  const auto back = read_back<xp_bodies>(out);
  CHECK_EQ(back.plain, "  x  "sv);
  CHECK_EQ(back.spaced.body, "  spaced out  "sv);
  CHECK_EQ(back.raw.inner, "\n  <a>1</a>\n  <b/>\n"sv);
  CHECK_EQ(back.script, "if(a<b){}"sv);
}

TEST_CASE("reflex::serde::xml pretty: an all-omitted aggregate stays on one line")
{
  CHECK_EQ(pretty<2>(xp_empty{}), "<xp_empty></xp_empty>"sv);
  CHECK_EQ(pretty<2>(xp_empty{3, {}}), "<xp_empty>\n  <maybe>3</maybe>\n</xp_empty>"sv);
}

TEST_CASE("reflex::serde::xml pretty: namespaces")
{
  const xp_env value{
      5, {80, "h"}
  };
  const std::string out = pretty<2>(value);
  CHECK_EQ(
      out, "<x:xp_env xmlns:x=\"http://e\">\n"
           "  <x:id>5</x:id>\n"
           "  <x:srv port=\"80\">\n"
           "    <host>h</host>\n"
           "  </x:srv>\n"
           "</x:xp_env>"sv);
  CHECK_EQ(read_back<xp_env>(out), value);
}

TEST_CASE("reflex::serde::xml pretty: a user override lands on its own line")
{
  const xp_with_user value{1, {7}, 2};
  const std::string  out = pretty<2>(value);
  CHECK_EQ(out, "<xp_with_user>\n  <a>1</a>\n  <u v=\"7\"/>\n  <b>2</b>\n</xp_with_user>"sv);
  CHECK_EQ(read_back<xp_with_user>(out), value);
}

TEST_CASE("reflex::serde::xml pretty: a scalar root has no break")
{
  CHECK_EQ(pretty<2>(42), "<int>42</int>"sv);
}
