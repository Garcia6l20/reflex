#include <reflex/di.hpp>
#include <reflex/views/permutations.hpp>

#include <functional>
#include <print>

using namespace reflex;

template <typename T>
concept printer_c = requires(T v) { v.print(); };

template <typename T>
concept dumber_c = requires(T v) { v.dumb(); };

template <dumber_c Dumb> struct print_service
{
  print_service(Dumb& dumb)
  {
    std::println("✔️  print_service created");
    dumb.dumb();
  }
  void print()
  {
    std::println("✔️  hello !");
  }
};

struct dumb_service
{
  dumb_service()
  {
    std::println("✔️  dumb_service created");
  }
  void dumb()
  {
    std::println("✔️  dumb !");
  }
};

template <printer_c Printer> struct one_dep_service
{
  one_dep_service(Printer& printer) : p{printer}
  {
    std::println("✔️  one_dep_service created");
    p.print();
  }
  Printer& p;
};

namespace registry
{
static constexpr auto dumb_srvc  = ^^dumb_service;
static constexpr auto print_srvc = ^^print_service;
} // namespace registry

#include <reflex/testing_main.hpp>

namespace di_concept_tests
{

struct test_base
{
  di::injector<di::config{.registry = ^^registry, .debug = true}> injector;
  void                                                            test_debugging()
  {
    using di::detail::dependencies_of;
    using di::detail::resolve_template_instance;
    {
      constexpr auto test = resolve_template_instance<^^dumb_service, ^^registry>();
      std::println("{}", display_string_of(test));
      injector.make_shared<^^dumb_service>();
    }
    {
      constexpr auto test = resolve_template_instance<^^print_service, ^^registry>();
      std::println("{}", display_string_of(test));
      injector.make_shared<^^print_service>();

      template for(constexpr auto tmp : define_static_array(dependencies_of<^^print_service, ^^registry>()))
      {
        std::println("{}", display_string_of(tmp));
      }
    }
    {
      constexpr auto test = resolve_template_instance<^^one_dep_service, ^^registry>();
      std::println("{}", display_string_of(test));

      template for(constexpr auto tmp : define_static_array(dependencies_of<^^one_dep_service, ^^registry>()))
      {
        std::println(" -- {}", display_string_of(tmp));
      }

      injector.make_shared<^^one_dep_service>();
    }
  }

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
} // namespace di_concept_tests
