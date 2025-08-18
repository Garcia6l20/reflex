#include <reflex/var.hpp>

#include <exception>
#include <print>

using namespace reflex;

#include <reflex/testing_main.hpp>

namespace var_constexpr_tests
{
using var_t = var<int, bool, double>;

static_assert(constexpr_constructible_c<var_t>);
static_assert(not constexpr_constructible_c<std::map<std::string, std::string>>);

using string_rvar = recursive_var<std::string>;

static_assert(not constexpr_constructible_c<string_rvar, string_rvar::map_t>);

void test_assign()
{
  constexpr var_t v = 42.2;
  static_assert(v == 42.2);
  constexpr auto v2 = v;
}
void test_var_of_vars()
{
  constexpr var_t v           = 42.2;
  using var_of_vars_t         = var<bool, var_t>;
  constexpr var_of_vars_t vov = v;
  static_assert(vov.has_value<var_t>());
  static_assert(vov.get<var_t>() == 42.2);
}

} // namespace var_constexpr_tests