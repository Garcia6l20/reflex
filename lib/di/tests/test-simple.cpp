#include <reflex/di.hpp>

#include <print>

using namespace reflex;

struct no_dep
{
  no_dep()
  {
    std::println("✔️  no_dep created");
  }
};

struct one_sptr_dep
{
  one_sptr_dep(std::shared_ptr<no_dep> nds)
  {
    std::println("✔️  one_sptr_dep created: shared[no_dep]@{:p}", static_cast<void*>(nds.get()));
  }
};

struct two_sptr_deps
{
  two_sptr_deps(std::shared_ptr<no_dep> nds, std::shared_ptr<one_sptr_dep> ods)
  {
    std::println("✔️  two_sptr_deps created: shared[no_dep]@{:p}", static_cast<void*>(nds.get()));
  }
};

struct one_uptr_dep
{
  one_uptr_dep(std::unique_ptr<no_dep> nds)
  {
    std::println("✔️  one_uptr_dep created");
  }
};

struct one_value_dep
{
  one_value_dep(no_dep nds)
  {
    std::println("✔️  one_value_dep created");
  }
};

struct one_ref_dep
{
  one_ref_dep(no_dep& nds)
  {
    std::println("✔️  one_ref_dep created: shared[no_dep]@{:p}", static_cast<void*>(&nds));
  }
};

struct one_value_one_ref_dep
{
  one_value_one_ref_dep(no_dep nds, no_dep& snd)
  {
    std::println("✔️  one_value_one_ref_dep created: shared[no_dep]@{:p}", static_cast<void*>(&snd));
  }
};

template <meta::info I, size_t indent = 0> void print_deps()
{
  using namespace di::detail;
  std::print("{:>{}s} {}", indent == 0 ? "➜" : "➥", indent, display_string_of(I));
  if constexpr(indent >= 50)
  {
    std::println(" error too much recursion");
  }
  else
  {
    constexpr auto deps = define_static_array(dependencies_of<I, ^^di::detail::empty_registry>());
    if constexpr(deps.empty())
    {
      std::println(": No dependencies");
    }
    else
    {
      std::println(", dependencies:");
      template for(constexpr auto dep : deps)
      {
        constexpr auto dep_obj = get_dependency_object_type(dep);
        print_deps<dep_obj, indent + 2>();
      }
    }
  }
}
namespace checks
{
using di::detail::dependency_type;
using di::detail::get_dependency_type;

static_assert(get_dependency_type(^^std::unique_ptr<no_dep>) == dependency_type::unique);
static_assert(get_dependency_type(^^no_dep) == dependency_type::unique);
static_assert(get_dependency_type(^^no_dep&&) == dependency_type::unique);

static_assert(get_dependency_type(^^std::shared_ptr<no_dep>) == dependency_type::shared);
static_assert(get_dependency_type(^^no_dep&) == dependency_type::shared);
static_assert(get_dependency_type(^^no_dep const&) == dependency_type::shared);
} // namespace checks

#include <reflex/testing_main.hpp>

namespace di_simple_tests
{

void test_base()
{
  di::injector<di::config{.debug = true}> injector;

  // print_deps<^^no_dep>();
  // print_deps<^^one_sptr_dep>();
  // print_deps<^^two_sptr_deps>();

  auto nds = injector.make_shared<no_dep>();
  std::println("{}", display_string_of(type_of(^^nds)));
  auto ods = injector.make_shared<one_sptr_dep>();
  std::println("{}", display_string_of(type_of(^^ods)));
  auto tds = injector.make_shared<two_sptr_deps>();
  std::println("{}", display_string_of(type_of(^^tds)));

  print_deps<^^one_uptr_dep>();
  auto oud = injector.make_unique<one_uptr_dep>();

  print_deps<^^one_value_dep>();
  auto ovd = injector.make_value<one_value_dep>();

  print_deps<^^one_ref_dep>();
  auto ord = injector.make_value<one_ref_dep>();

  auto ovord = injector.make_value<one_value_one_ref_dep>();
}
} // namespace di_simple_tests
