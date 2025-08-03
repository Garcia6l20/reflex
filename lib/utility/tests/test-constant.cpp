#include <reflex/constant.hpp>

#include <reflex/testing_main.hpp>

using namespace reflex;

namespace constant_tests
{
  namespace arithmetic_tests
  {
    template <constant V> struct use_int_constant
    {
      static constexpr int v = V;
    };
    void test()
    {
      STATIC_CHECK_THAT(use_int_constant<42>::v == 42);
    }
  }
  namespace string_tests
  {
    template <constant V> struct use_string_constant
    {
      static constexpr std::string_view v = V;
    };
    void test()
    {
      using namespace std::literals;
      constexpr auto i = use_string_constant<"hello"s>{};
      std::println("s = {}", i.v);
      STATIC_CHECK_THAT(i.v == "hello");
    }
  }
  namespace string_view_tests
  {
    template <constant V> struct use_string_constant
    {
      static constexpr std::string_view v = V;
    };
    void test()
    {
      using namespace std::literals;
      STATIC_CHECK_THAT(use_string_constant<"hello"sv>::v == "hello");
    }
  }
  namespace string_literal_tests
  {
    // NOTE inner type (ie.: char[N]) cannot be specified
    template <string_literal_constant V> struct use_string_constant
    {
      // v is std::string_view
      static constexpr std::string_view v = V;
    };
    void test()
    {
      STATIC_CHECK_THAT(use_string_constant<"hello">::v == "hello");
    }
  }
  namespace vector_tests
  {
    template <constant V> struct use_vec
    {
      using value_type                              = typename std::decay_t<decltype(V)>::value_type;
      static constexpr bool has_span_representation = template_of(^^value_type) == ^^std::span;
      static constexpr auto v                       = V.get();
    };
    void test()
    {
      {
        static constexpr auto with_vec = use_vec<std::vector{1, 2, 3}>{};
        std::println("v = {}", with_vec.v);
        STATIC_CHECK_THAT(std::ranges::equal(with_vec.v, std::array{1, 2, 3}));
        STATIC_CHECK_THAT(with_vec.has_span_representation);
      }
      {
        static constexpr auto with_array = use_vec<std::array{1, 2, 3}>{};
        STATIC_CHECK_THAT(std::ranges::equal(with_array.v, std::array{1, 2, 3}));
        STATIC_CHECK_THAT(not with_array.has_span_representation); // array uses structural representation
      }
    }
  }
}
