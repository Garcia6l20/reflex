
#include <reflex/regitry.hpp>

#include <reflex/testing_main.hpp>

using namespace reflex;

struct scope
{
};
using reg = meta::registry<scope>;

namespace meta_registry_tests
{
using namespace reflex::meta::registry_detail;
void test1()
{
  consteval
  {
    reg::add(^^bool);
  };
  STATIC_CHECK_THAT(count<scope>() == 1);
  STATIC_CHECK_THAT(registered_type<scope, 0> == ^^bool);
  (void)registerer<float, scope>{};
  STATIC_CHECK_THAT(count<scope>() == 2);
  STATIC_CHECK_THAT(registered_type<scope, 1> == ^^float);
}

struct self_registered1 : registerer<self_registered1, scope>
{
};

struct self_registered2 : reg::auto_<self_registered2>
{
};

void test2()
{
  template for(constexpr auto t : reg::all())
  {
    std::println("- {}", display_string_of(t));
  }
  STATIC_CHECK_THAT(registered_type<scope, 2> == ^^self_registered1);
  STATIC_CHECK_THAT(registered_type<scope, 3> == ^^self_registered2);
}
} // namespace meta_registry_tests

REFLEX_LOCK_REGISTRY(scope)

// template <> const reg::runtime_t reg::runtime = reg::lock();

namespace meta_registry_locked_tests
{

void test_lock_reg()
{
  for(auto const& entry : reg::runtime.entries)
  {
    std::println("- {}", entry.name);
  }
}
} // namespace meta_registry_locked_tests
