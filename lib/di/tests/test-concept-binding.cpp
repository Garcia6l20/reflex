#include <reflex/di.hpp>
#include <reflex/views/combinations.hpp>
#include <reflex/views/permutations.hpp>

#include <catch2/catch_test_macros.hpp>

#include <functional>
#include <print>

using namespace reflex;

struct print_service
{
  print_service()
  {
    std::println("✔️  print_service created");
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

template <typename T>
concept printer_c = requires(T v) { v.print(); };

template <typename T>
concept dumber_c = requires(T v) { v.dumb(); };

template <printer_c Printer> struct one_dep_service
{
  one_dep_service(std::shared_ptr<Printer> printer)
  {
    std::println("✔️  one_dep_service created");
    printer->print();
  }
};

template <meta::info parent> consteval bool is_ctor_of(meta::info I)
{
  return (is_constructor(I) or is_constructor_template(I)) and is_user_provided(I);
}

#define dump_classification(__elem__, __classif__) \
  std::println("{}.{}: {}", #__elem__, #__classif__, __classif__(__elem__))

template <meta::info T, auto Fn> struct binder
{
  static constexpr meta::info   target = T;
  static constexpr decltype(Fn) fn     = Fn;
};

struct any_arg
{
  template <typename U> operator U();
};

consteval bool is_constructible_type_with_n_args(meta::info obj, size_t N)
{
  std::vector<meta::info> args;
  args.resize(N);
  std::ranges::fill(args, ^^any_arg);
  return is_constructible_type(obj, args);
}

consteval auto any_template_ctor_type(meta::info obj, size_t N) -> meta::info
{
  std::vector<meta::info> args;
  args.resize(N);
  std::ranges::fill(args, ^^any_arg);
  auto obj_template_inst = substitute(obj, args);
  return obj_template_inst;
}

template <meta::info obj, meta::info registry_ns, size_t indent = 0> void print_deps()
{
  using namespace di::detail;
  std::println("{:>{}s} {}", indent == 0 ? "➜" : "➥", indent, display_string_of(obj));

  constexpr auto registry      = define_static_array(load_registry_namespace<registry_ns>());
  constexpr auto registry_size = registry.size();
  std::println("registry size: {}", registry_size);

  if constexpr(is_template(obj))
  {
    template for(constexpr auto args_count : std::views::iota(size_t(0), registry.size()))
    {
      template for(constexpr auto p : define_static_array(registry | views::permutations<args_count>))
      {
        std::print("checking permutation ");
        template for(constexpr auto i : p)
        {
          std::print("{}, ", display_string_of(i));
        }
        std::println();
        if constexpr(can_substitute(obj, p))
        {
          constexpr auto obj_template_inst = substitute(obj, p);
          std::println("using template instance {}", display_string_of(obj_template_inst));
          print_deps<obj_template_inst, registry_ns>();
          return;
        }
        else
        {
          std::println("cannot create template instance");
        }
      }
      // using TI = T<any_arg>;
      // if constexpr(is_constructible_type_with_n_args(obj, args_count))
      // {
      // }
    }
  }
  else
  {
    constexpr auto ctx = meta::access_context::current();
    template for(constexpr auto ctor : define_static_array(members_of(obj, ctx)                  //
                                                           | std::views::filter(is_ctor_of<obj>) //
                                                           ))
    {
      if constexpr(meta::is_constructor(ctor))
      {
        std::println("ctor: {}", display_string_of(ctor));
        template for(constexpr auto p : define_static_array(parameters_of(ctor)))
        {
          constexpr auto ptype = type_of(p);
          if constexpr(has_template_arguments(ptype))
          {
            if constexpr(template_of(ptype) == ^^std::shared_ptr or template_of(ptype) == ^^std::unique_ptr)
            {
              constexpr auto dtype = template_arguments_of(ptype)[0];
              print_deps<dtype, registry_ns, indent + 2>();
            }
            else
            {
              std::println(": Unhandled template: {}", display_string_of(ptype));
            }
          }
          else
          {
            std::println(": Unhandled type: {}", display_string_of(ptype));
          }
        }
      }
      else if constexpr(is_constructor_template(ctor))
      {
        std::println("template-ctor: {}", display_string_of(ctor));

        // template for(constexpr auto args_count : std::views::iota(0, 2))
        // {
        //   if constexpr(is_constructible_type_with_n_args(obj, args_count))
        //   {
        //     template for(constexpr auto p : define_static_array(registry | views::permutations<args_count>))
        //     {
        //       // using ObjT = [:obj:];
        //       // constexpr auto tmp = ^^(ObjT::ObjT<any_arg>);

        //       using CtorT = [:ctor:];
        //       // constexpr auto tmp = ^^(CtorT<any_arg>);

        //       constexpr auto tmp = any_template_ctor_type<obj, args_count>();
        //       constexpr auto sz  = p.size();
        //       std::print("  checking ({} items): ", sz);
        //       // constexpr auto get_arg = []<size_t I> constexpr
        //       // {
        //       //   constexpr auto type = std::get<I>(registry);
        //       //   using T             = [:type:];
        //       //   return ^^std::shared_ptr<T>;
        //       // };
        //       // constexpr auto make_args = [&]<size_t... I>(std::index_sequence<I...>) constexpr
        //       // { return std::array{get_arg.template operator()<I>()...}; }(std::make_index_sequence<args_count>());

        //       template for(constexpr auto item : p)
        //       {
        //         std::print("{}, ", display_string_of(item));
        //       }
        //       std::println();
        //       // is_invocable_type()
        //     }
        //   }
        // }
      }
      else
      {
        // std::println("non-ctor: {}", display_string_of(ctor));
      }
    }
  }
}

namespace registry
{
static constexpr auto dumb_srvc  = ^^dumb_service;
static constexpr auto print_srvc = ^^print_service;
} // namespace registry

TEST_CASE("base-test")
{
  {
    static constexpr auto input = std::array{1, 2, 3, 7};
    template for(constexpr auto p : define_static_array(input | views::permutations<2>))
    {
      std::println("- {}, {}", std::get<0>(p), std::get<1>(p));
    }
  }

  // template for (constexpr auto p : define_static_array(get_bindings_permutations<1>(bindings))) {
  //   std::println("{}", display_string_of(type_of(^^p)));
  // }

  print_deps<^^one_dep_service, ^^registry>();
}
