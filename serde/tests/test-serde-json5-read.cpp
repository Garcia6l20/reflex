#include <doctest/doctest.h>

import reflex.serde.json;

import std;

using namespace reflex;
using namespace reflex::serde;

struct jr_inner
{
  int         port;
  std::string host;

  constexpr bool operator==(jr_inner const&) const = default;
};

struct jr_cfg
{
  int                     value;
  jr_inner                srv;
  std::optional<jr_inner> backup;
  std::vector<int>        list;
  bool                    on;

  constexpr bool operator==(jr_cfg const&) const = default;
};

struct jr_url
{
  std::string url;

  constexpr bool operator==(jr_url const&) const = default;
};

constexpr json::read_options jr_comments{.comments = true};
constexpr json::read_options jr_trailing{.trailing_commas = true};

static jr_cfg jr_expected()
{
  return {
      .value = 1, .srv = {8080, "a"},
           .backup = std::nullopt, .list = {1,    2  },
           .on = true
  };
}

template <typename T> static T jr_load_comments(std::string const& text)
{
  return json::deserializer{text, std::cw<jr_comments>}.template load<T>();
}

template <typename T> static T jr_load_comments_stream(std::string const& text)
{
  std::istringstream in{text};
  return json::deserializer{in, std::cw<jr_comments>}.template load<T>();
}

template <typename T> static T jr_load_trailing(std::string const& text)
{
  return json::deserializer{text, std::cw<jr_trailing>}.template load<T>();
}

template <typename T> static T jr_load_default(std::string const& text)
{
  return json::deserializer{text}.template load<T>();
}

static const std::string jr_commented = R"(// header
/* block header */
{ // after brace
  // before value
  "value" /* between key and colon */ : /* between colon and value */ 1// right after a number
  , // after comma
  "srv": {
    "port": 8080/* block right after a number */,
    "host": "a" // holds " and // and /* inside
    // before closing brace
  },
  /* multi
     line */
  "backup": null,
  "list": [ // in array
    1, /* between */ 2 // last
  ],
  "on": /**/ true
}
// footer
/* trailing block */
)";

TEST_CASE("reflex::serde::json5 read: comments before, between and after every token")
{
  CHECK(jr_load_comments<jr_cfg>(jr_commented) == jr_expected());
  CHECK(jr_load_comments_stream<jr_cfg>(jr_commented) == jr_expected());
}

TEST_CASE("reflex::serde::json5 read: a comment at document start before a scalar")
{
  CHECK_EQ(jr_load_comments<int>("// x\n42"), 42);
  CHECK_EQ(jr_load_comments<std::string>("/* x */ \"s\""), "s");
  CHECK_EQ(jr_load_comments<bool>("// x\ntrue"), true);
  CHECK_EQ(jr_load_comments<std::vector<int>>("// x\n[1]"), std::vector<int>{1});
  CHECK_EQ(jr_load_comments_stream<int>("// x\n42"), 42);
}

TEST_CASE("reflex::serde::json5 read: malformed comments")
{
  CHECK_THROWS_WITH(jr_load_comments<jr_inner>("{/* open"), "Unterminated JSON comment");
  CHECK_THROWS_WITH(jr_load_comments_stream<jr_inner>("{/* open *"), "Unterminated JSON comment");
  CHECK_THROWS_WITH(jr_load_comments<jr_inner>("{ / }"), "Unexpected '/' in JSON");
  CHECK_THROWS_WITH(jr_load_comments<jr_inner>("{\"port\": 1 /"), "Unexpected '/' in JSON");
}

TEST_CASE("reflex::serde::json5 read: a // inside a string is string content")
{
  CHECK(jr_load_comments<jr_url>(R"({"url":"http://x"})") == jr_url{"http://x"});
  CHECK(jr_load_comments<jr_url>(R"({"url":"/* no */"})") == jr_url{"/* no */"});
  CHECK(jr_load_comments_stream<jr_url>(R"({"url":"http://x"})") == jr_url{"http://x"});
}

TEST_CASE("reflex::serde::json5 read: a json::value document")
{
  const auto doc = jr_load_comments<json::value>(R"(// h
{ "a": [1, /* x */ 2], // y
  "b": { "c": "d" } /* z */ }
)");
  REQUIRE(doc.is_object());
  CHECK_EQ(doc.at("b.c")->as<std::string>(), "d");
  CHECK_EQ(doc.at("a")->as_array().size(), 2);
}

TEST_CASE("reflex::serde::json5 read: trailing commas")
{
  CHECK_EQ(jr_load_trailing<std::vector<int>>("[1,2,]"), std::vector<int>{1, 2});
  CHECK_EQ(jr_load_trailing<std::vector<int>>("[1, 2 , ]"), std::vector<int>{1, 2});
  CHECK(jr_load_trailing<jr_inner>(R"({"port":1,"host":"h",})") == jr_inner{1, "h"});
  CHECK(jr_load_trailing<jr_inner>(R"({"port":1,"host":"h" , })") == jr_inner{1, "h"});
  CHECK_THROWS(jr_load_trailing<std::vector<int>>("[,]"));
  CHECK_THROWS(jr_load_trailing<jr_inner>("{,}"));
  CHECK_THROWS(jr_load_trailing<std::vector<int>>("[1,,]"));
}

TEST_CASE("reflex::serde::json5 read: both flags together")
{
  constexpr json::read_options both{.comments = true, .trailing_commas = true};
  const std::string            text = "{\"port\": 1, // p\n \"host\": \"h\", // h\n}";
  CHECK(json::deserializer{text, std::cw<both>}.load<jr_inner>() == jr_inner{1, "h"});
}

TEST_CASE("reflex::serde::json5 read: defaults refuse comments and trailing commas")
{
  CHECK_THROWS(jr_load_default<jr_cfg>(jr_commented));
  CHECK_THROWS(jr_load_default<int>("// x\n42"));
  CHECK_THROWS_WITH(jr_load_default<int>("1// x"), "Failed to parse number");
  CHECK_THROWS(jr_load_default<std::vector<int>>("[1,2,]"));
  CHECK_THROWS(jr_load_default<jr_inner>(R"({"port":1,"host":"h",})"));
  CHECK_THROWS(jr_load_comments<std::vector<int>>("[1,2,]"));
  CHECK_THROWS(jr_load_trailing<jr_inner>("{\"port\":1 // x\n,\"host\":\"h\"}"));
}
