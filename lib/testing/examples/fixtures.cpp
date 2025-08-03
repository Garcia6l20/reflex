#include <reflex/testing_main.hpp>
#include <reflex/format/variant.hpp>

using namespace reflex::testing;

namespace fixture_tests
{

const std::tuple<std::string_view, std::string_view> hello_fixture[] = {
    {"hello", "world"},
    {"hello", "hello"},
    {"world", "world"},
};

[[= parametrize<^^hello_fixture>]] void test_hello(std::string_view l, std::string_view r)
{
  CHECK_THAT(l == r); // l and r will be expanded since they are added to execution context
}

// template-functions are also supported !
[[= parametrize<^^hello_fixture>]] void test_hello_template(auto l, auto r)
{
  CHECK_THAT(l == r);
}

using var = std::variant<int, double>;

const std::tuple<var, var> var_fixture[] = {
    {42, 42.3},
    {42, 42.2},
    {42.2, 42.3},
    {42, 42.2},
    {42, 42},
    {42.2, 42.2},
};
[[= parametrize<^^var_fixture>]] void test_var_template(auto l, auto r)
{
  // for now l and r are variants, but it is planned to unwrap them into its actual type
  CHECK_THAT(l == r);
}

} // namespace fixture_tests
