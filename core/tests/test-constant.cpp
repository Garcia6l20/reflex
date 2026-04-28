#include <doctest/doctest.h>

#include <reflex/constant.hpp>

using namespace reflex;

struct agg_of_structural_types
{
  int  x;
  bool y;

  constexpr bool operator==(agg_of_structural_types const& other) const = default;
  constexpr auto operator<=>(agg_of_structural_types const& other) const = default;
};

struct agg_of_non_structural_types
{
  int  x;
  constant_string y;

  constexpr bool operator==(agg_of_non_structural_types const& other) const = default;
  constexpr auto operator<=>(agg_of_non_structural_types const& other) const = default;
};

TEST_CASE("reflex::constant: basics")
{
    SUBCASE("int")
    {
        static constexpr auto c1 = constant{42};
        static_assert(c1 == 42);
        static_assert(*c1 == 42);
        static_assert(constant{42} == c1);
        static_assert(constant{43} != c1);
        // Check that it's formattable via std::format as its underlying type
        CHECK(std::format("{}", c1) == "42");
        CHECK(std::format("{:08x}", c1) == "0000002a");
    }
    SUBCASE("aggregate of structural types")
    {
        static constexpr auto c2 = constant{agg_of_structural_types{1, true}};
        static_assert(c2->x == 1);
        static_assert(c2->y == true);
        static_assert(constant{agg_of_structural_types{1, true}} == c2);
        static_assert(constant{agg_of_structural_types{2, false}} != c2);
    }
    SUBCASE("aggregate of non-structural types")
    {
        static constexpr auto c3 = constant{agg_of_non_structural_types{1, "hello"}};
        static_assert(c3->x == 1);
        static_assert(c3->y == "hello");
        static_assert(constant{agg_of_non_structural_types{1, "hello"}} == c3);
        static_assert(constant{agg_of_non_structural_types{2, "world"}} != c3);
    }
}


TEST_CASE("reflex::constant: vector")
{
  SUBCASE("vector of int")
  {
    static constexpr auto c1 = constant{std::vector<int>{42, 43}};
    static_assert(c1->size() == 2);
    static_assert(c1->at(0) == 42);
    static_assert(c1->back() == 43);
  }
  SUBCASE("vector of aggregate of structural types")
  {
    static constexpr auto c2 = constant{std::vector<agg_of_structural_types>{{1, true}, {2, false}}};
    static_assert(c2->size() == 2);
    static_assert(c2->at(0).x == 1);
    static_assert(c2->at(0).y == true);
    static_assert(c2->at(1).x == 2);
    static_assert(c2->at(1).y == false);
  }
  SUBCASE("vector of aggregate of non-structural types")
  {
    static constexpr auto c3 = constant{std::vector<agg_of_non_structural_types>{{1, "hello"}, {2, "world"}}};
    static_assert(c3->size() == 2);
    static_assert(c3->at(0).x == 1);
    static_assert(c3->at(0).y == "hello");
    static_assert(c3->at(1).x == 2);
    static_assert(c3->at(1).y == "world");
  }
}

template <constant_string Str>
struct use_nttp_string
{
  static_assert(Str == "hello");
};
using example_nttp_string = use_nttp_string<"hello">;

template <constant<std::vector<int>> Seq> struct use_nttp_sequence {
  static_assert(Seq->at(0) == 1);
  static_assert(Seq->at(1) == 2);
  static_assert(Seq->at(2) == 3);
};

using example_nttp_sequence = use_nttp_sequence<std::vector{1, 2, 3}>;
