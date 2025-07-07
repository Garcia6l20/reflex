#include <reflex/di.hpp>
#include <reflex/views/combinations.hpp>
#include <reflex/views/permutations.hpp>

#include <catch2/catch_test_macros.hpp>

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

TEST_CASE("base-test")
{
  std::println("===============================================================================");

  di::injector<di::config{.registry = ^^registry, .debug = true}> injector;

  // SECTION("debugging")
  // {
  //   using di::detail::dependencies_of;
  //   using di::detail::resolve_template_instance;
  //   {
  //     constexpr auto test = resolve_template_instance<^^dumb_service, ^^registry>();
  //     std::println("{}", display_string_of(test));
  //     injector.make_shared<^^dumb_service>();
  //   }
  //   {
  //     constexpr auto test = resolve_template_instance<^^print_service, ^^registry>();
  //     std::println("{}", display_string_of(test));
  //     injector.make_shared<^^print_service>();

  //     template for(constexpr auto tmp : define_static_array(dependencies_of<^^print_service, ^^registry>()))
  //     {
  //       std::println("{}", display_string_of(tmp));
  //     }
  //   }
  //   {
  //     constexpr auto test = resolve_template_instance<^^one_dep_service, ^^registry>();
  //     std::println("{}", display_string_of(test));

  //     template for(constexpr auto tmp : define_static_array(dependencies_of<^^one_dep_service, ^^registry>()))
  //     {
  //       std::println(" -- {}", display_string_of(tmp));
  //     }

  //     injector.make_shared<^^one_dep_service>();
  //   }
  // }

  SECTION("shared objects are persistant")
  {
    auto ods1 = injector.make_shared<^^one_dep_service>();
    auto ods2 = injector.make_shared<^^one_dep_service>();
    REQUIRE(ods1 == ods2);
  }
  SECTION("unique objects are not persistant")
  {
    auto ods1 = injector.make_unique<^^one_dep_service>();
    auto ods2 = injector.make_unique<^^one_dep_service>();
    REQUIRE(ods1 != ods2);
    THEN("shared dependencies are actually shared")
    {
      REQUIRE(&ods1->p == &ods2->p);
    }
  }
}
