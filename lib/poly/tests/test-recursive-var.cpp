#include <reflex/format/memory.hpp> // for printing shared_ptr...
#include <reflex/var.hpp>

#include <exception>
#include <print>

#include <flat_map>

using namespace reflex;

#include <reflex/testing_main.hpp>

namespace recursive_var_tests
{
struct test_vector
{
  using var_t = recursive_var<int, bool, std::string>;
  using vec_t = var_t::vec_t;
  static_assert(is_recursive_type(^^recursive<std::vector>));
  var_t c;

  static_assert(var_t::can_hold<int>());
  static_assert(var_t::can_hold<std::string>());
  static_assert(var_t::can_hold<std::vector<var_t>>());
  static_assert(var_t::can_hold<std::vector>());
  static_assert(var_value_c<int, var_t>);

  void test_empty()
  {
    CHECK_THAT(c.has_value()).negate();
  }

  void test_copy_constructible()
  {
    c = vec_t{1};
    ASSERT_THAT(c.has_value());
    ASSERT_THAT(c.has_value<vec_t>());
    var_t b = c;
    CHECK_CONTEXT(b);
    CHECK_THAT(c[0] == 1);
    CHECK_THAT(b[0] == 1);
    CHECK_THAT(b) == expr(c);
    c.push_back(55);
    CHECK_THAT(b) != expr(c);
  }

  void test_move_constructible()
  {
    var_t v = vec_t{1};
    auto *p = v.vec().data();
    c = std::move(v);
    ASSERT_THAT(v.vec().size() == 0);
    ASSERT_THAT(c.has_value());
    ASSERT_THAT(c.has_value<vec_t>());
    CHECK_THAT(c) != expr(v);
    CHECK_THAT(p) == expr(c.vec().data());
  }

  void test_base()
  {
    c = "hello";
    std::println("{}", c);

    std::println("{}", c.get<std::string>());
    c = vec_t{1, "2", 3};
    // printable element
    {
      auto& tmp = c.get<std::vector>()[0];
      // std::println("{}", display_string_of(type_of(^^tmp)));
      std::println("{}", display_string_of(type_of(^^tmp)));
      std::println("{}", tmp.get<int>());
      std::println("{}", tmp);
    }
    std::println("{:j}", c);
    std::println("{:j}", c[1]);
    CHECK_THAT(c[0]) == 1;
    CHECK_THAT(c[1]) == "2";
    CHECK_THAT(c[2]) == 3;
    CHECK_THAT(c.at(0)) == 1;
    CHECK_THAT(c.at(1)) == "2";
    CHECK_THAT(c.at(2)) == 3;
    for(auto const& v : c.vec())
    {
      visit(match{[](std::string const& v) { std::println("- string: \"{}\"", v); }, //
                    [](int const& v) { std::println("- int: {}", v); },                //
                    [](auto const& v)
                    {
                      std::println("unexpected {} ({})", v, display_string_of(type_of(^^v)));
                      std::unreachable();
                    }},
            v);
    }
    CHECK_THAT(c) == vec_t{1, "2", 3};
  }
  void test_implicit_init()
  {
    c[0] = 42; // vec implied
    c[3] = 43; // implicitly adds 2 x none
    c.push_back(true);
    std::println("vec: {:jp}", c);
    CHECK_THAT(c[0]) == 42;
    CHECK_THAT(c[1]) == none;
    CHECK_THAT(c[2]) == none;
    CHECK_THAT(c[3]) == 43;
    CHECK_THAT(c[4]) == true;
  }
};

struct test_map
{
  using var_t = recursive_var<int, bool, std::string>;
  using map_t = var_t::map_t;
  var_t c;

  void test_base()
  {
    std::println("{}", display_string_of(dealias(^^map_t)));
    c = map_t{{"1", 1}, {"2", "2"}, {"3", 3}};
    CHECK_THAT(c.has_value<map_t>());
    CHECK_THAT(c.has_value<std::map>());
    auto& tmp = c["1"];
    std::println("{}", tmp);
    CHECK_THAT(c["1"]) == 1;
    CHECK_THAT(c["2"]) == "2";
    CHECK_THAT(c["3"]) == 3;
    CHECK_THAT(c.at("1")) == 1;
    CHECK_THAT(c.at("2")) == "2";
    CHECK_THAT(c.at("3")) == 3;
    std::println("{:j}", c);
    for(auto const& [k, v] : c.map())
    {
      visit(match{[&k](std::string const& v) { std::println("- string: \"{}\" -> \"{}\"", k, v); }, //
                    [&k](int const& v) { std::println("- int: \"{}\" -> {}", k, v); },                //
                    [&k](auto const& v)
                    {
                      std::println("unexpected {} -> {} ({})", k, v, display_string_of(type_of(^^v)));
                      std::unreachable();
                    }},
            v);
    }
    CHECK_THAT(c) == map_t{{"1", 1}, {"2", "2"}, {"3", 3}};
  }
  void test_implicit_init()
  {
    c["hello"] = "world"; // map implied
    c["value"] = 42;
    c["none"]  = none;
    std::println("{:j4}", c); // pretty print implied
    c["nested"]["int"] = 42;
    c["nested"]["str"] = "yup";
    // std::println("{:j12}", c); // format_error: indent value has only 1 digit
    std::println("{:jp}", c); // pretty print with default indent
  }
};

struct S
{
  int value;
};

struct test_raw_pointer
{
  using var_t = var<int, bool, S, S*, void*>;
  var_t c;

  void test1()
  {
    c = static_cast<void*>(nullptr);
    CHECK_THAT(c.has_value<void*>());
    CHECK_THAT(c == static_cast<void*>(nullptr));
    std::println("{}", c);
  }
};

consteval bool is_invocable_lambda(auto fn, std::initializer_list<meta::info> types)
{
  for(auto M :
      members_of(type_of(^^fn), meta::access_context::current()) | std::views::filter(meta::is_function_template))
  {
    if(can_substitute(M, types))
    {
      return true;
    }
  }
  return false;
}

#define REFLEX_EXPRESSION_CHECKER(__args__, __body__) \
  [] __args__                                         \
    requires requires __body__ {}
#define REFLEX_IS_VALID_EXPRESSION(__args__, __body__, __types__) \
  is_invocable_lambda(REFLEX_EXPRESSION_CHECKER(__args__, __body__), __types__)

static_assert(is_invocable_lambda(REFLEX_EXPRESSION_CHECKER((auto v), { v.data(); }), {^^std::string}));
static_assert(REFLEX_IS_VALID_EXPRESSION((auto v), { v.data(); }, {^^std::string}));
static_assert(not REFLEX_IS_VALID_EXPRESSION((auto v), { v.data(); }, {^^bool}));

static_assert(is_invocable_lambda([](auto v)
                                    requires requires { v.data(); } {},
                                  {^^std::string}));
static_assert(not is_invocable_lambda([](auto v)
                                        requires requires { v.data(); } {},
                                      {^^bool}));

void test()
{
  int  a = 42;
  auto f = [&a](auto v) { v.data(); };
  template for(constexpr auto M : define_static_array(members_of(type_of(^^f), meta::access_context::current()) |
                                                      std::views::filter(meta::is_function_template)))
  {
    if constexpr(can_substitute(M, {^^std::string}))
    {
      std::println("{}", display_string_of(M));
    }
  }
}

struct test_ptr
{
  using var_t = var<bool, int, std::string, recursive<std::shared_ptr>>;

  var_t c;

  void test_base()
  {
    c = var_t::recursive_t<std::shared_ptr>{new var_t{42}};
    std::println("{}", c);
    // c = std::shared_ptr<var_t>{new var_t{42}};
    // CHECK_THAT(*c == 42);
  }
};
} // namespace recursive_var_tests
