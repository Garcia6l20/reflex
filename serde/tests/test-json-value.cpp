#include <doctest/doctest.h>

import reflex.serde.json;

import std;

using namespace reflex;
using namespace reflex::serde;

TEST_CASE("serde::json::value: constructs")
{
  SUBCASE("null")
  {
    json::value v = json::null;
    CHECK(v.is_null());
    CHECK(v == json::null);
  }

  SUBCASE("boolean")
  {
    json::value t = true;
    json::value f = false;
    CHECK(t.is_boolean());
    CHECK(t == true);
    CHECK(f == false);
  }

  SUBCASE("number - double")
  {
    json::value v = 3.14;
    CHECK(v.is_number());
    CHECK(v == 3.14);
  }

  SUBCASE("number - integral promotion")
  {
    json::value v = 42; // int → number
    CHECK(v.is_number());
    CHECK(v == 42);

    json::value u = std::size_t{7};
    CHECK(u.is_number());
    CHECK(u == 7);
  }

  SUBCASE("string")
  {
    json::value v = std::string{"hello"};
    CHECK(v.is_string());
    CHECK(v == "hello");
  }

  SUBCASE("array - initializer list")
  {
    json::array arr{1, 2.0, std::string{"three"}};
    json::value v = arr;
    CHECK(v.is_array());
    CHECK(v.size() == 3);
  }

  SUBCASE("object - initializer list")
  {
    json::value v = {
        {"name", std::string{"alice"}},
        {"age",  30                  }
    };
    CHECK(v.is_object());
    CHECK(v.size() == 2);
  }
}

TEST_CASE("serde::json::value: as<T> and get<T>")
{
  SUBCASE("as<T> const")
  {
    json::value v = std::string{"world"};
    CHECK(v.as<json::string>() == "world");
  }

  SUBCASE("as<T> mutable")
  {
    json::value v = std::string{"foo"};
    v.as<json::string>() += "bar";
    CHECK(v == "foobar");
  }

  SUBCASE("get<T> returns value when type matches")
  {
    json::value v   = 1.5;
    auto        opt = v.get<json::number>();
    REQUIRE(opt.has_value());
    CHECK(opt.value() == doctest::Approx(1.5));
  }

  SUBCASE("get<T> returns nullopt when type mismatches")
  {
    json::value v   = true;
    auto        opt = v.get<json::number>();
    CHECK_FALSE(opt.has_value());
  }
}

TEST_CASE("serde::json::value: array access")
{
  json::value v = json::array{10, 20, 30};

  SUBCASE("operator[] by index")
  {
    CHECK(v[0] == 10);
    CHECK(v[1] == 20);
    CHECK(v[2] == 30);
  }

  SUBCASE("push_back")
  {
    v.push_back(40);
    CHECK(v.size() == 4);
    CHECK(v[3] == 40);
  }

  SUBCASE("size / empty")
  {
    CHECK(v.size() == 3);
    CHECK_FALSE(v.empty());

    json::value empty = json::array{};
    CHECK(empty.empty());
  }
}

TEST_CASE("serde::json::value: object access")
{
  json::value v = {
      {"name",   std::string{"bob"}},
      {"score",  99                },
      {"active", true              }
  };

  SUBCASE("operator[] single key")
  {
    CHECK(v["name"] == "bob");
    CHECK(v["score"] == 99);
    CHECK(v["active"] == true);
  }

  SUBCASE("missing key returns null")
  {
    CHECK(v["missing"] == json::null);
  }

  SUBCASE("contains")
  {
    CHECK(v.contains("name"));
    CHECK_FALSE(v.contains("nope"));
  }

  SUBCASE("size / empty")
  {
    CHECK(v.size() == 3);
    CHECK_FALSE(v.empty());
  }

  SUBCASE("mutable operator[]")
  {
    v["score"].as<json::number>() = 100.0;
    CHECK(v["score"] == 100);
  }
}

TEST_CASE("serde::json::value: dotted path access")
{
  json::value v = {
      {"user",
       json::value{
           {"name", std::string{"carol"}},
           {"address",
            json::value{{"city", std::string{"Paris"}}, {"country", std::string{"France"}}}}}},
      {"score", 42                                                                           }
  };

  SUBCASE("single level")
  {
    CHECK(v["score"] == 42);
  }

  SUBCASE("two levels")
  {
    CHECK(v["user.name"] == "carol");
  }

  SUBCASE("three levels")
  {
    CHECK(v["user.address.city"] == "Paris");
    CHECK(v["user.address.country"] == "France");
  }

  SUBCASE("missing intermediate returns null")
  {
    CHECK(v["user.nope.city"] == json::null);
  }

  SUBCASE("missing leaf returns null")
  {
    CHECK(v["user.age"] == json::null);
  }

  SUBCASE("contains with dotted path")
  {
    CHECK(v.contains("user.name"));
    CHECK(v.contains("user.address.city"));
    CHECK_FALSE(v.contains("user.phone"));
    CHECK_FALSE(v.contains("user.address.zip"));
  }
}

TEST_CASE("serde::json::value: merge")
{
  json::value v = {
      {"a", 1},
      {"b", 2}
  };
  json::object extra = {
      {"b", 99},
      {"c", 3 }
  };

  v.merge(extra);

  CHECK(v["a"] == 1);
  CHECK(v["b"] == 99); // overwritten
  CHECK(v["c"] == 3);  // inserted
}

TEST_CASE("serde::json::value: visit")
{
  SUBCASE("dispatches to correct type")
  {
    json::value v      = std::string{"hi"};
    bool        called = false;
    v.visit(
        match{
            [&](json::string const& s) {
              called = true;
              CHECK(s == "hi");
            },
            [](auto const&) { FAIL("wrong type"); },
        });
    CHECK(called);
  }

  SUBCASE("visit number")
  {
    json::value v = 7.0;
    v.visit(
        match{
            [](json::number n) { CHECK(n == doctest::Approx(7.0)); },
            [](auto const&) { FAIL("wrong type"); },
        });
  }
}

TEST_CASE("serde::json::value: size and empty for null/scalar")
{
  CHECK(json::value{json::null}.size() == 0);
  CHECK(json::value{json::null}.empty() == true);
  CHECK(json::value{42}.size() == 1);
  CHECK(json::value{42}.empty() == false);
  CHECK(json::value{std::string{"ab"}}.size() == 2); // string char count
}

TEST_CASE("serde::json::value: formattable")
{
  SUBCASE("null")
  {
    auto result = std::format("{}", json::value{json::null});
    CHECK(result == "null");
  }
}
