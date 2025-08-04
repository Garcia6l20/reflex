#include <reflex/constant.hpp>

#include <reflex/testing_main.hpp>

using namespace reflex;
using namespace std::literals;

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
} // namespace arithmetic_tests
namespace string_tests
{
template <constant V> struct use_string_constant
{
  static constexpr std::string_view v = V;
};
void test()
{
  constexpr auto i = use_string_constant<"hello"s>{};
  std::println("s = {}", i.v);
  STATIC_CHECK_THAT(i.v == "hello");
}
} // namespace string_tests
namespace string_view_tests
{
template <constant V> struct use_string_constant
{
  static constexpr std::string_view v = V;
};
void test()
{
  STATIC_CHECK_THAT(use_string_constant<"hello"sv>::v == "hello");
}
} // namespace string_view_tests
namespace string_literal_tests
{
template <constant V> struct use_string_constant
{
  static constexpr std::string_view v = V;
};
void test()
{
  use_string_constant<"hello"> c;
  std::println("{}", c.v.size());
  STATIC_CHECK_THAT(c.v == "hello");
  {
    constexpr use_string_constant<"hello"sv> c2;
    STATIC_CHECK_THAT(c2.v == "hello");
    const void* p  = c.v.data();
    const void* p2 = c2.v.data();
    CHECK_THAT(p == p2);
  }
  {
    constexpr use_string_constant<"hello"s> c2;
    STATIC_CHECK_THAT(c2.v == "hello");
    const void* p  = c.v.data();
    const void* p2 = c2.v.data();
    CHECK_THAT(p == p2);
  }
}
} // namespace string_literal_tests
namespace range_tests
{
template <constant V> struct use_range
{
  using value_type                              = typename std::decay_t<decltype(V)>::value_type;
  static constexpr bool has_span_representation = template_of(^^value_type) == ^^std::span;
  static constexpr auto v                       = V.get();
};
void test()
{
  {
    static constexpr auto with_vec = use_range<std::vector{1, 2, 3}>{};
    std::println("v = {}", with_vec.v);
    STATIC_CHECK_THAT(std::ranges::equal(with_vec.v, std::array{1, 2, 3}));
    STATIC_CHECK_THAT(with_vec.has_span_representation);
  }
  {
    static constexpr auto with_array = use_range<std::array{1, 2, 3}>{};
    STATIC_CHECK_THAT(std::ranges::equal(with_array.v, std::array{1, 2, 3}));
    STATIC_CHECK_THAT(not with_array.has_span_representation); // array uses structural representation
  }
}
void test_deduction()
{
  static constexpr auto with_array = use_range<{1, 2, 3}>{};
  STATIC_CHECK_THAT(std::ranges::equal(with_array.v, std::array{1, 2, 3}));
  STATIC_CHECK_THAT(not with_array.has_span_representation); // array uses structural representation
}
} // namespace vector_tests
namespace tuple_tests
{
template <constant V> struct use_tuple
{
  static constexpr auto v = V.get();
};
void test_ints()
{
  static constexpr auto v = use_tuple<std::tuple{42, 43}>{};
  static_assert(std::get<0>(v.v) == 42);
  static_assert(std::get<1>(v.v) == 43);
}
void test_strings()
{
  static constexpr auto v = use_tuple<std::tuple{42, "hello"}>{};
  static_assert(std::get<0>(v.v) == 42);
  static_assert(std::get<1>(v.v) == "hello");
  // a unique "hello" constant shall be defined
  const void* p  = std::get<1>(v.v).data();
  const void* p2 = constant_string{"hello"}->data();
  CHECK_THAT(p == p2);
  CHECK_THAT(std::get<1>(v.v).data() == constant_string{"hello"}->data());
}
void test_deduction()
{
  static constexpr auto v1 = use_tuple<{42, true}>{};
  static constexpr auto v2 = use_tuple<{42, "hello"}>{};
}
} // namespace tuple_tests
namespace variant_tests
{
using var = std::variant<bool, int, std::string_view>;
template <constant<var> V> struct use_variant
{
  static constexpr auto v = V.get();
};
void test_int()
{
  static constexpr auto v = use_variant<42>{};
  static_assert(std::get<int>(v.v) == 42);
}
void test_str()
{
  static constexpr auto v = use_variant<"hello">{};
  static_assert(std::get<std::string_view>(v.v) == "hello");
}
} // namespace variant_tests
} // namespace constant_tests
