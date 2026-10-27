#include <doctest/doctest.h>

import reflex.serde.json;
import reflex.serde.toml;
import reflex.serde.yaml;

import std;

using namespace reflex;
using namespace reflex::serde;
using namespace std::literals;

struct xf_srv
{
  std::string host;
  int         port = 0;

  bool operator==(xf_srv const&) const = default;
};

struct xf_config
{
  std::string title;
  int         port  = 0;
  bool        debug = false;
  xf_srv      srv;

  bool operator==(xf_config const&) const = default;
};

constexpr std::string_view xf_yaml = R"(# Application configuration

# instance name
title: demo # shown in the title bar
# port to listen on
port: 8080
debug: false # never in production

# upstream server
srv: # the public listener
  # host name or address
  host: localhost
  port: 9000 # upstream port
  # end of srv

# end of file
)";

static commented<xf_config> xf_from_yaml()
{
  yaml::deserializer de{xf_yaml, std::cw<yaml::read_options{.comments = true}>};
  return de.load_commented<xf_config>();
}

TEST_CASE("reflex::serde comments cross-format: YAML read keeps every comment")
{
  const auto [cfg, c] = xf_from_yaml();
  const xf_config expected{
      .title = "demo", .port = 8080, .srv = {.host = "localhost", .port = 9000}
  };
  CHECK_EQ(cfg, expected);
  CHECK_EQ(comment(c).leading, std::vector<std::string>{"Application configuration", "\n"});
  CHECK_EQ(comment(c).footer, std::vector<std::string>{"\n", "end of file"});
  CHECK_EQ(c.title.leading, std::vector<std::string>{"instance name"});
  CHECK_EQ(c.title.trailing, "shown in the title bar");
  CHECK_EQ(c.port.leading, std::vector<std::string>{"port to listen on"});
  CHECK_EQ(c.debug.trailing, "never in production");
  CHECK_EQ(comment(c.srv).leading, std::vector<std::string>{"\n", "upstream server"});
  CHECK_EQ(comment(c.srv).trailing, "the public listener");
  CHECK_EQ(comment(c.srv).footer, std::vector<std::string>{"end of srv"});
  CHECK_EQ(c.srv.host.leading, std::vector<std::string>{"host name or address"});
  CHECK_EQ(c.srv.port.trailing, "upstream port");
  CHECK_EQ(layout_of(c), layout{.indent = 2});
}

TEST_CASE("reflex::serde comments cross-format: YAML to JSON5 and back keeps the tree")
{
  const auto [cfg, c] = xf_from_yaml();

  std::string out;
  json::serializer{out, std::cw<json::options{.indent = 2, .comments = true}>}.dump(cfg, c);
  INFO(out);

  json::deserializer de{out, std::cw<json::read_options{.comments = true}>};
  const auto [back, back_c] = de.load_commented<xf_config>();
  CHECK_EQ(back, cfg);
  CHECK(back_c == c);
}

TEST_CASE("reflex::serde comments cross-format: YAML to TOML and back moves only what TOML must")
{
  const auto [cfg, c] = xf_from_yaml();

  std::string out;
  toml::serializer{out, std::cw<toml::options{.comments = true}>}.dump(cfg, c);
  INFO(out);

  toml::deserializer de{out, std::cw<toml::read_options{.comments = true}>};
  const auto [back, back_c] = de.load_commented<xf_config>();
  CHECK_EQ(back, cfg);

  CHECK(comment(back_c.srv).footer.empty());
  CHECK_EQ(comment(back_c).footer, std::vector<std::string>{"end of srv", "\n", "end of file"});
  CHECK_EQ(layout_of(back_c).indent, 0);

  auto expected                = c;
  layout_of(expected).indent   = 0;
  comment(expected.srv).footer = {};
  comment(expected).footer     = comment(back_c).footer;
  CHECK(back_c == expected);
}
