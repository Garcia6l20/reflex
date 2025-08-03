#include <reflex/di.hpp>
#include <reflex/views/permutations.hpp>

#include <functional>
#include <print>

using namespace reflex;

struct print_interface
{
  virtual void print() = 0;
};

struct dumb_interface
{
  virtual void dumb() = 0;
};

struct print_service : print_interface
{
  print_service(dumb_interface& dumb)
  {
    std::println("✔️  print_service created");
    dumb.dumb();
  }
  void print() override
  {
    std::println("✔️  hello !");
  }
};

struct dumb_service : dumb_interface
{
  dumb_service()
  {
    std::println("✔️  dumb_service created");
  }
  void dumb() override
  {
    std::println("✔️  dumb !");
  }
};

struct one_dep_service
{
  one_dep_service(print_interface& printer) : p{printer}
  {
    std::println("✔️  one_dep_service created");
    p.print();
  }
  print_interface& p;
};

namespace registry
{
static constexpr auto dumb_srvc  = ^^dumb_service;
static constexpr auto print_srvc = ^^print_service;
} // namespace registry

#include <reflex/testing_main.hpp>

namespace di_interface_tests
{

struct test_base
{
  di::injector<di::config{.registry = ^^registry, .debug = true}> injector;

  void test_shared_objects_are_persistant()
  {
    auto ods1 = injector.make_shared<^^one_dep_service>();
    auto ods2 = injector.make_shared<^^one_dep_service>();
    CHECK_THAT(ods1) == ods2;
  }
  void test_unique_objects_are_not_persistant()
  {
    auto ods1 = injector.make_unique<^^one_dep_service>();
    auto ods2 = injector.make_unique<^^one_dep_service>();
    CHECK_THAT(ods1) != ods2;
    CHECK_THAT(&ods1->p) == &ods2->p;
  }
};
} // namespace di_interface_tests
