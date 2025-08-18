#include <reflex/testing_main.hpp>

#include <reflex/concepts.hpp>
#include <reflex/none.hpp>

namespace reflex
{

template <typename...> union variadic_union;

template <> union variadic_union<>
{
};

template <typename Head, typename... Tail> union variadic_union<Head, Tail...>
{
public:
  constexpr variadic_union() noexcept : dummy_{}
  {
  }
  constexpr variadic_union(auto_of(Head) value) noexcept : head_{std::forward<Head>(value)}
  {
  }

  template <typename T>
    requires(not decays_to_c<T, Head>)
  constexpr variadic_union(T&& value) noexcept : tail_{std::forward<T>(value)}
  {
  }

  constexpr ~variadic_union() noexcept
  {
  }

  constexpr variadic_union& operator=(auto_of(Head) value) noexcept
  {
    head_ = std::forward<Head>(value);
    return *this;
  }

  template <typename T>
    requires(not decays_to_c<T, Head>)
  constexpr variadic_union& operator=(T&& value) noexcept
  {
    tail_ = std::forward<T>(value);
    return *this;
  }

  template <typename T>
    requires(std::same_as<T, Head>)
  constexpr decltype(auto) get(this auto &&self) noexcept
  {
    return std::forward_like<decltype(self)>(self.head_);
  }

  template <typename T>
    requires(not std::same_as<T, Head>)
  constexpr decltype(auto) get(this auto &&self) noexcept
  {
    return std::forward_like<decltype(self)>(self.tail_).template get<T>();
  }

  constexpr variadic_union(const variadic_union&)            = default;
  constexpr variadic_union(variadic_union&&)                 = default;
  constexpr variadic_union& operator=(const variadic_union&) = default;
  constexpr variadic_union& operator=(variadic_union&&)      = default;

  char                    dummy_;
  Head                    head_;
  variadic_union<Tail...> tail_;
};

// template <typename ...Ts>
// auto make_variadic_union()
// {
//   union impl_t;
//   consteval
//   {
//     define_aggregate(^^impl_t,
//                      std::array{^^Ts...} | std::views::transform([](auto R) { return data_member_spec(R, {}); }));
//   };
//   return variadic_union_impl2<impl_t>{};
// }

} // namespace reflex

namespace variadic_union_tests
{
using namespace reflex;
// void test1()
// {
//   union impl_t;
//   consteval
//   {
//     define_aggregate(^^impl_t,
//                      std::array{^^none_t, ^^int, ^^std::string} | std::views::transform([](auto R) { return
//                      data_member_spec(R, {}); }));
//   };
//   impl_t i;
//   std::println("");
// }
void test2()
{
  // variadic_union_impl<int, bool, std::string> u;

  // requires(std::ranges::any_of(std::array{^^Ts...},
  //          [](auto R) { return not is_trivially_destructible_type(R); }

  auto u1 = variadic_union<int, bool>{};
  // static_assert(is_trivially_destructible_type(^^decltype(u1)));
  auto u2 = variadic_union<int, bool, std::string>{};
  static_assert(not is_trivially_destructible_type(^^decltype(u2)));
  u2 = std::string{"hello"};
  // u2.values.__2 = std::string{};

  std::println("{}", u2.get<std::string>());
}
} // namespace variadic_union_tests
