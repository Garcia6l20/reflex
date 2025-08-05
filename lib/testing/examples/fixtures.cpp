#include <reflex/format/variant.hpp>
#include <reflex/testing_main.hpp>

using namespace reflex::testing;

namespace fixture_tests
{

constexpr std::tuple<std::string_view, std::string_view> hello_fixture[] = {
    {"hello", "world"},
    {"hello", "hello"},
    {"world", "world"},
};

[[= parametrize<^^hello_fixture>]] void test_hello(std::string_view l, std::string_view r)
{
  CHECK_THAT(l == r); // l and r will be expanded since they are added to execution context
}

using var = std::variant<int, double>;

using var_fixture_row_type = std::array<var, 2>;
// using var_fixture_row_type = std::tuple<var, var>; // functionally equivalent
constexpr var_fixture_row_type var_fixture[] = {
    {42, 42.3},
    {42, 42.2},
    {42.2, 42.3},
    {42, 42.2},
    {42, 42},
    {42.2, 42.2},
};
[[= parametrize<^^var_fixture>]] void test_var_template(auto&& l, auto&& r)
{
  CHECK_THAT(l == r);
  STATIC_CHECK_THAT(type_of(^^l) == type_of(^^r));
}

constexpr auto tuple_fixture = std::tuple{
    std::tuple{42, 42.3},
    std::tuple{42, 42.2},
    std::tuple{42.2, 42.3},
    std::tuple{42, 42.2},
    std::tuple{42, 42},
    std::tuple{42.2, 42.2},
};

struct struct_based_test
{
  [[= parametrize<^^tuple_fixture>]] void test(auto&& l, auto&& r)
  {
    CHECK_THAT(l == r);
    STATIC_CHECK_THAT(type_of(^^l) == type_of(^^r));
  }
};
} // namespace fixture_tests
