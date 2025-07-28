#include <reflex/var.hpp>

#include <exception>
#include <print>

#include <catch2/catch_test_macros.hpp>

#include <flat_map>

using namespace reflex;

TEST_CASE("vector")
{
  using var_t = recursive_var<int, bool, std::string>;
  var_t c;
  static_assert(c.can_hold<int>());
  static_assert(c.can_hold<std::string>());
  static_assert(c.can_hold<std::vector<var_t>>());
  static_assert(c.can_hold<std::vector>());
  static_assert(var_value_c<int, var_t>);

  SECTION("base")
  {
    c = "hello";
    std::println("{}", c);

    std::println("{}", c.get<std::string>());
    c = var_t::vec_t{1, "2", 3};
    SECTION("printable element")
    {
      auto& tmp = c.get<std::vector>()[0];
      // std::println("{}", display_string_of(type_of(^^tmp)));
      std::println("{}", display_string_of(type_of(^^tmp)));
      std::println("{}", tmp.get<int>());
      std::println("{}", tmp);
    }
    std::println("{:j}", c);
    std::println("{:j}", c[1]);
    REQUIRE(c[0] == 1);
    REQUIRE(c[1] == "2");
    REQUIRE(c[2] == 3);
    REQUIRE(c.at(0) == 1);
    REQUIRE(c.at(1) == "2");
    REQUIRE(c.at(2) == 3);
    for(auto const& v : c.vec())
    {
      v.match([](std::string const& v) { std::println("- string: \"{}\"", v); }, //
              [](int const& v) { std::println("- int: {}", v); },                //
              [](auto const& v)
              {
                std::println("unexpected {} ({})", v, display_string_of(type_of(^^v)));
                std::unreachable();
              });
    }
    REQUIRE(c == var_t::vec_t{1, "2", 3});
  }
  SECTION("implicit init")
  {
    c[0] = 42; // vec implied
    c[3] = 43; // implicitly adds 2 x none
    c.push_back(true);
    std::println("vec: {:jp}", c);
    REQUIRE(c[0] == 42);
    REQUIRE(c[1] == none);
    REQUIRE(c[2] == none);
    REQUIRE(c[3] == 43);
    REQUIRE(c[4] == true);
  }
}

TEST_CASE("map")
{
  using var_t = recursive_var<int, bool, std::string>;
  var_t c;

  SECTION("base")
  {
    std::println("{}", display_string_of(dealias(^^var_t::map_t)));
    c = var_t::map_t{{"1", 1}, {"2", "2"}, {"3", 3}};
    REQUIRE(c.has_value<var_t::map_t>());
    REQUIRE(c.has_value<std::map>());
    auto& tmp = c["1"];
    std::println("{}", tmp);
    REQUIRE(c["1"] == 1);
    REQUIRE(c["2"] == "2");
    REQUIRE(c["3"] == 3);
    REQUIRE(c.at("1") == 1);
    REQUIRE(c.at("2") == "2");
    REQUIRE(c.at("3") == 3);
    std::println("{:j}", c);
    for(auto const& [k, v] : c.map())
    {
      v.match([&k](std::string const& v) { std::println("- string: \"{}\" -> \"{}\"", k, v); }, //
              [&k](int const& v) { std::println("- int: \"{}\" -> {}", k, v); },                //
              [&k](auto const& v)
              {
                std::println("unexpected {} -> {} ({})", k, v, display_string_of(type_of(^^v)));
                std::unreachable();
              });
    }
    REQUIRE(c == var_t::map_t{{"1", 1}, {"2", "2"}, {"3", 3}});
  }
  SECTION("implicit init")
  {
    c["hello"] = "world"; // map implied
    c["value"] = 42;
    c["none"] = none;
    std::println("{:j4}", c); // pretty print implied
    c["nested"]["int"] = 42;
    c["nested"]["str"] = "yup";
    // std::println("{:j12}", c); // format_error: indent value has only 1 digit
    std::println("{:jp}", c); // pretty print with default indent
  }
}
