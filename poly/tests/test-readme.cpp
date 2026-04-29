#include <doctest/doctest.h>

import reflex.poly;
import std;

using namespace reflex;
using namespace reflex::poly;
using namespace std::string_literals;

// A JSON-like value type
using value  = var<bool, std::int64_t, double, std::string>;
using array  = value::arr_type; // arr<bool, std::int64_t, double, std::string>
using object = value::obj_type; // obj<bool, std::int64_t, double, std::string>

TEST_CASE("reflex::poly::var: README")
{
  value v1 = 42;       // integral — promoted to int64_t
  value v2 = 3.14;     // floating point
  value v3 = "hello"s; // string
  value v4 = null;     // null
  value v5 = {
      // object (initializer-list syntax)
      {"key",  1       },
      {"name", "Alice"s},
  };
  value v6 = array{1, 2, 3}; // array

  SUBCASE("Type predicates")
  {
    CHECK(v1.template is<std::int64_t>()); // true
    CHECK(v4.is_null());                   // true
    CHECK(v5.is_object());                 // true
    CHECK(v6.is_array());                  // true
  }

  SUBCASE("Typed access")
  {
    [[maybe_unused]] auto n =
        v1.template as<std::int64_t>();      // throws std::bad_variant_access on mismatch
    auto s = v3.template get<std::string>(); // → std::optional<std::string&>

    CHECK(v5["key"] == 1);               // object subscript — returns var&
    CHECK(v5.at("key") == 1);            // → std::optional<var const&>  (non-throwing)
    CHECK_FALSE(v5.contains("missing")); // → false
  }

  SUBCASE("Dotted-path access")
  {
    value cfg = {
        {"db", {{"host", "localhost"s}, {"port", 5432}}}
    };
    CHECK(cfg["db.host"] == "localhost"); // → var holding "localhost"
    CHECK(cfg.contains("db.port"));       // → true
  }

  SUBCASE("Visitation")
  {
    v1.visit(
        reflex::match{
            [](std::int64_t i) { std::println("int: {}", i); },
            [](std::string& s) { std::println("str: {}", s); },
            [](auto const&) { std::println("other"); },
        });
  }

  SUBCASE("std::format support")
  {
    std::println("{}", v5);   // {key:1,name:Alice}
    std::println("{}", v6);   // [1,2,3]
    std::println("{}", null); // null
  }

  SUBCASE("Reference wrapping")
  {
    using value = var<bool, std::int64_t, double, std::string, std::string&>;

    std::string str = "hello";
    value       ref = std::ref(str);             // holds std::string*
    CHECK(ref.template is<std::string&>());      // true
    ref.template as<std::string&>() += " world"; // str == "hello world"
    CHECK(str == "hello world");
  }
}
