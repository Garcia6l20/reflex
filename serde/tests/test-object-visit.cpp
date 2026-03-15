#include <doctest/doctest.h>

import reflex.core;
import reflex.poly;
import reflex.serde;

import std;

using namespace reflex;
using namespace reflex::poly;
using namespace reflex::serde;
using namespace std::string_literals;

using value  = var<bool, double, std::string>;
using array  = value::arr_type;
using object = value::obj_type;

TEST_CASE("serde::object_visit: var")
{
  SUBCASE("simple")
  {
    object obj = {
        {"name", "alice"s},
        {"age",  30      }
    };
    bool visited = false;
    object_visit("age", obj, [&visited](auto const& value) {
      CHECK(value == 30);
      visited = true;
    });
    CHECK(visited);
  }
  SUBCASE("recursive")
  {
    object obj = {
        {"alice",
         object{
             {"age", 30},
             {"address", object{{"city", "Wonderland"s}, {"zip", 12345}}},
         }},
        {"bob",
         object{
             {"age", 66},
             {"address", object{{"city", "Badlands"s}, {"zip", 666}}},
         }},
    };
    {
      bool visited = false;
      object_visit("alice.age", obj, [&visited](auto const& value) {
        std::println("Alice's age: {}", value);
        CHECK(value == 30);
        visited = true;
      });
      CHECK(visited);
    }
    {
      bool visited = false;
      object_visit("bob.address.city", obj, [&visited](auto const& value) {
        std::println("Bob's city: {}", value);
        CHECK(value == "Badlands");
        visited = true;
      });
      CHECK(visited);
    }
  }
}

struct address
{
  std::string city;
  int         zip;
};

struct person
{
  std::string name;
  int         age;
  address     addr;
};

TEST_CASE("serde::object_visit: aggregates")
{
  person alice{
      .name = "alice"s,
      .age  = 30,
      .addr =
          {
                 .city = "Wonderland"s,
                 .zip  = 12345,
                 },
  };
  SUBCASE("simple")
  {
    bool visited = false;
    object_visit("age", alice, [&visited](auto const& value) {
      if constexpr(decay(type_of(^^value)) == ^^int)
      {
        CHECK(value == 30);
        visited = true;
      }
      else
      {
        FAIL("Expected int");
      }
    });
    CHECK(visited);
  }
  SUBCASE("recursive")
  {
    {
      bool visited = false;
      object_visit("addr.city", alice, [&visited](auto const& value) {
        if constexpr(decay(type_of(^^value)) == dealias(^^std::string))
        {
          // std::println("Alice's city: {}", value);
          CHECK(value == "Wonderland");
          visited = true;
        }
        else
        {
          FAIL("Expected string");
        }
      });
      CHECK(visited);
    }
  }
}
