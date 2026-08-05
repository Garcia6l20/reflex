#include <doctest/doctest.h>

#include <reflex/core.hpp>

#include <string>

namespace ops
{
  struct vec
  {
    int x = 0;

    // binary, two overloads under one operator
    vec operator+(const vec& other) const { return vec{x + other.x}; }
    vec operator+(int v) const { return vec{x + v}; }

    // unary, sharing its symbol with the binary minus
    vec operator-() const { return vec{-x}; }
    vec operator-(const vec& other) const { return vec{x - other.x}; }

    // subscript, const and mutable
    int&       operator[](int) { return x; }
    const int& operator[](int) const { return x; }

    // call operator with a defaulted parameter
    int operator()(int mul = 2) const { return x * mul; }
  };

  // a free operator, in the same namespace
  vec operator*(const vec& a, int k)
  {
    return vec{a.x * k};
  }
  vec operator*(int k, const vec& a)
  {
    return vec{a.x * k};
  }
} // namespace ops

TEST_CASE("reflex::resolve: operators")
{
  ops::vec a{3};
  ops::vec b{4};

  SUBCASE("a member operator is named by its spelling")
  {
    CHECK_EQ(reflex::resolve<^^ops::vec, "operator+">(a, b).x, 7);
    CHECK_EQ(reflex::resolve<^^ops::vec, "operator+">(a, 10).x, 13);
  }
  SUBCASE("a unary and a binary sharing a symbol are told apart by the arguments")
  {
    CHECK_EQ(reflex::resolve<^^ops::vec, "operator-">(a).x, -3);
    CHECK_EQ(reflex::resolve<^^ops::vec, "operator-">(b, a).x, 1);
  }
  SUBCASE("the object constness picks the subscript overload")
  {
    const ops::vec ca{9};
    CHECK_EQ(reflex::resolve<^^ops::vec, "operator[]">(a, 0), 3);
    CHECK_EQ(reflex::resolve<^^ops::vec, "operator[]">(ca, 0), 9);
    static_assert(
        std::same_as<std::invoke_result_t<decltype(reflex::resolve<^^ops::vec, "operator[]">),
                                          ops::vec&, int>,
                     int&>);
  }
  SUBCASE("the call operator resolves like any other, defaults included")
  {
    CHECK_EQ(reflex::resolve<^^ops::vec, "operator()">(a), 6);
    CHECK_EQ(reflex::resolve<^^ops::vec, "operator()">(a, 5), 15);
  }
  SUBCASE("a free operator resolves through its namespace")
  {
    constexpr auto mul = reflex::resolve<^^ops, "operator*">;
    CHECK_EQ(mul(a, 3).x, 9);
    CHECK_EQ(mul(3, a).x, 9);
  }
  SUBCASE("a call matching no operator is not invocable")
  {
    constexpr auto plus = reflex::resolve<^^ops::vec, "operator+">;
    static_assert(not std::invocable<decltype(plus), ops::vec&>);
    static_assert(not std::invocable<decltype(plus), ops::vec&, void*>);
  }
}

TEST_CASE("reflex::overloads_of: operators")
{
  SUBCASE("every overload of one operator is listed")
  {
    constexpr auto count = reflex::overloads_of(^^ops::vec, "operator+").size();
    static_assert(count == 2);
  }
  SUBCASE("a unary and a binary under one symbol are both listed")
  {
    constexpr auto all = [] consteval {
      return reflex::overloads_of(^^ops::vec, "operator-");
    };
    static_assert(all().size() == 2);
    static_assert(all()[0].arity == 0);
    static_assert(all()[1].arity == 1);
  }
  SUBCASE("a free operator is listed under its namespace")
  {
    constexpr auto count = reflex::overloads_of(^^ops, "operator*").size();
    static_assert(count == 2);
  }
  SUBCASE("an operator that is not declared yields no candidate")
  {
    static_assert(reflex::overloads_of(^^ops::vec, "operator/").empty());
  }
}
