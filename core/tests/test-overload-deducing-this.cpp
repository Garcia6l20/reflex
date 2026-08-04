#include <doctest/doctest.h>

#include <reflex/core.hpp>

namespace
{
  struct gadget
  {
    int n = 1;

    // deducing this, taking the object by const reference
    int peek(this const gadget& self) { return self.n; }

    // a deducing this overload next to an ordinary one, under one name
    int mix(this const gadget& self, int v) { return self.n + v; }
    int mix(this const gadget& self, double v) { return self.n + static_cast<int>(v) * 100; }

    // by value, so any object works
    int copied(this gadget self) { return self.n * 2; }

    // a defaulted parameter, past the object one
    int scaled(this const gadget& self, int mul = 3) { return self.n * mul; }
  };

  struct mutating
  {
    int n = 0;

    // needs a mutable object
    int bump(this mutating& self) { return ++self.n; }

    // needs an expiring one
    int consume(this mutating&& self) { return self.n + 50; }
  };
} // namespace

TEST_CASE("reflex::resolve: explicit object parameter")
{
  gadget g{};

  SUBCASE("the object parameter is the object, not an argument")
  {
    CHECK_EQ(reflex::resolve<^^gadget, "peek">(g), 1);
    constexpr auto peek = reflex::resolve<^^gadget, "peek">;
    static_assert(std::invocable<decltype(peek), gadget&>);
    static_assert(not std::invocable<decltype(peek), gadget&, int>);
  }
  SUBCASE("overloads are ranked on the arguments that remain")
  {
    CHECK_EQ(reflex::resolve<^^gadget, "mix">(g, 2), 3);
    CHECK_EQ(reflex::resolve<^^gadget, "mix">(g, 2.0), 201);
  }
  SUBCASE("an object taken by value accepts anything")
  {
    const gadget cg{};
    CHECK_EQ(reflex::resolve<^^gadget, "copied">(g), 2);
    CHECK_EQ(reflex::resolve<^^gadget, "copied">(cg), 2);
  }
  SUBCASE("a defaulted parameter past the object may be omitted")
  {
    CHECK_EQ(reflex::resolve<^^gadget, "scaled">(g), 3);
    CHECK_EQ(reflex::resolve<^^gadget, "scaled">(g, 5), 5);
  }
  SUBCASE("a mutable object parameter rejects a const object")
  {
    constexpr auto bump = reflex::resolve<^^mutating, "bump">;
    mutating       m{};
    CHECK_EQ(bump(m), 1);
    static_assert(std::invocable<decltype(bump), mutating&>);
    static_assert(not std::invocable<decltype(bump), const mutating&>);
  }
  SUBCASE("an rvalue object parameter rejects an lvalue")
  {
    constexpr auto consume = reflex::resolve<^^mutating, "consume">;
    CHECK_EQ(consume(mutating{7}), 57);
    static_assert(std::invocable<decltype(consume), mutating&&>);
    static_assert(not std::invocable<decltype(consume), mutating&>);
  }
}

TEST_CASE("reflex::overloads_of: explicit object parameter")
{
  SUBCASE("the object parameter is not counted as an argument")
  {
    constexpr auto one = [] consteval {
      return reflex::overloads_of(^^gadget, "peek").front();
    }();
    static_assert(one.arity == 0);
    static_assert(one.parameter_types().empty());
    static_assert(one.return_type() == ^^int);
  }
  SUBCASE("the remaining parameters are reported")
  {
    constexpr auto all = [] consteval {
      return reflex::overloads_of(^^gadget, "mix");
    };
    static_assert(all().size() == 2);
    static_assert(all()[0].arity == 1);
    static_assert(all()[0].parameter_types().front() == ^^int);
    static_assert(all()[1].parameter_types().front() == ^^double);
  }
  SUBCASE("a defaulted parameter still expands into arities")
  {
    constexpr auto count = reflex::overloads_of(^^gadget, "scaled").size();
    static_assert(count == 2);
  }
}
