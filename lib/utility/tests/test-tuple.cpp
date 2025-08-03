#include <reflex/meta.hpp>

// #include <catch2/catch_test_macros.hpp>

using namespace reflex;

namespace reflex
{

namespace detail
{
template <typename... Ts> struct tuple_impl
{
  struct type;
  consteval
  {
    define_aggregate(^^type,
                     std::array{
                         ^^Ts...} |
                         std::views::transform([](auto R) { return data_member_spec(R, {}); }));
  };
};
} // namespace detail

template <typename... Ts> struct tuple;

template <typename T>
concept tuple_c = has_template_arguments(decay(^^T)) and (template_of(decay(^^T)) == ^^tuple);

template <typename T, typename... Ts>
concept tuple_with_c = tuple_c<T> and (template_arguments_of(decay(^^T)) == std::vector{^^Ts...});

template <typename... Ts> struct tuple : detail::tuple_impl<Ts...>::type
{
  using __type = detail::tuple_impl<Ts...>::type;

  static constexpr auto __members =
      define_static_array(nonstatic_data_members_of(^^__type, meta::access_context::current()));
  static constexpr auto size = __members.size();

  template <size_t I, typename Self> constexpr decltype(auto) get(this Self&& self) noexcept
  {
    return self.[:__members[I]:];
  }

  template <typename T, T index, typename Self>
  constexpr decltype(auto) get(this Self&& self, std::integral_constant<T, index> const&) noexcept
  {
    if constexpr(std::is_signed_v<T>)
    {
      if constexpr(index < 0)
      {
        return self.[:__members[size + index]:];
      }
      else
      {
        return self.[:__members[index]:];
      }
    }
    else
    {
      return self.[:__members[index]:];
    }
  }

  template <typename Fn, typename Self> constexpr decltype(auto) invoke(this Self&& self, Fn&& fn) noexcept
  {
    return [&]<size_t... I>(std::index_sequence<I...>)
    { return static_cast<Fn&&>(fn)(self.[:__members[I]:]...); }(std::make_index_sequence<size>());
  }

  template <typename... Us, typename Self> constexpr decltype(auto) append(this Self&& self, Us&&... values) noexcept
  {
    return [&]<size_t... I>(std::index_sequence<I...>)
    {
      return tuple<Ts..., Us...>{std::forward_like<Self>(self.[:__members[I]:])..., std::forward<Us>(values)...};
    }(std::make_index_sequence<size>());
  }

  template <tuple_c Other, typename Self> constexpr decltype(auto) concat(this Self&& self, Other&& other) noexcept
  {
    constexpr auto new_tuple_type = [] consteval
    {
      auto types = std::vector{^^Ts...};
      types.append_range(template_arguments_of(decay(^^Other)));
      return substitute(template_of(^^tuple), types);
    }();
    using ResultType = [:new_tuple_type:];
    return [&]<size_t... I>(std::index_sequence<I...>)
    {
      return [&]<size_t... J>(std::index_sequence<J...>)
      {
        return ResultType{std::forward_like<Self>(self.[:self.__members[I]:])...,
                          std::forward_like<Other>(other.[:other.__members[J]:])...};
      }(std::make_index_sequence<other.size>());
    }(std::make_index_sequence<size>());
  }

  // tie binding
  template <tuple_with_c<std::remove_reference_t<Ts>...> Other, typename Self>
  constexpr decltype(auto) operator=(this Self&& self, Other&& other) noexcept
  {
    [&]<size_t... I>(std::index_sequence<I...>)
    { ((self.[:self.__members[I]:] = other.[:other.__members[I]:]), ...); }(std::make_index_sequence<size>());
    return self;
  }
};
template <typename... Ts> tuple(Ts&&...) -> tuple<Ts...>;

template <std::size_t I, typename... Ts> inline constexpr decltype(auto) get(tuple<Ts...>& t) noexcept
{
  return t.template get<I>();
}
template <std::size_t I, typename... Ts> inline constexpr decltype(auto) get(tuple<Ts...> const& t) noexcept
{
  return t.template get<I>();
}
template <std::size_t I, typename... Ts> inline constexpr decltype(auto) get(tuple<Ts...>&& t) noexcept
{
  return std::move(t).template get<I>();
}

template <typename... Ts> auto tie(Ts&... values)
{
  return tuple<Ts&...>{values...};
}

namespace literals
{

namespace detail
{
template <typename T, char... Chars> consteval auto parse_integral()
{
  constexpr auto data = define_static_array(std::array{Chars...}                                      //
                                            | std::views::drop_while([](auto c) { return c == '0'; }) //
                                            | std::views::transform([](auto c) { return c - '0'; }));
  if constexpr(data.size() == 0)
  {
    return 0;
  }
  else
  {
    T value = data.front();
    if constexpr(data.size() > 1)
    {
      template for(constexpr auto c : data | std::views::drop(1))
      {
        value = (value * 10) + c;
      }
    }
    return value;
  }
}
} // namespace detail

template <char... Chars> constexpr auto operator""_uz()
{
  constexpr auto value = detail::parse_integral<size_t, Chars...>();
  return std::integral_constant<size_t, value>{};
}

template <char... Chars> constexpr auto operator""_sz()
{
  constexpr auto value = detail::parse_integral<std::ptrdiff_t, Chars...>();
  return std::integral_constant<std::ptrdiff_t, value>{};
}

// add unary minus operator to handle negative values (by default the are converted to value_type)
template <std::ptrdiff_t V> constexpr auto operator-(std::integral_constant<std::ptrdiff_t, V>)
{
  return std::integral_constant<std::ptrdiff_t, -V>{};
}

static_assert(1_uz == 1);
static_assert(10_uz == 10);
static_assert(100_uz == 100);
static_assert(1_sz == 1);
static_assert(10_sz == 10);
static_assert(-1_sz == -1);
static_assert(-100_sz == -100);
static_assert(-00100_sz == -100);

} // namespace literals

} // namespace reflex

namespace std
{

template <typename... Ts> struct tuple_size<reflex::tuple<Ts...>>
{
  static constexpr auto value = reflex::tuple<Ts...>::size;
};

template <size_t I, typename... Ts> struct tuple_element<I, reflex::tuple<Ts...>>
{
  using type = [:type_of(reflex::tuple<Ts...>::__members[I]):];
};

} // namespace std

using namespace reflex::literals;

#include <reflex/testing_main.hpp>

namespace tuple_tests
{

void base_test()
{
  constexpr auto t = tuple{42, true};
  ASSERT_THAT(tuple_c<decltype(t)>);
  ASSERT_THAT((tuple_with_c<decltype(t), int, bool>));

  std::println("==== {} ====", display_string_of(decay(type_of(^^t))));

  ASSERT_THAT(t.get<0>()) == 42;
  ASSERT_THAT(t.get<1>()) == true;

  ASSERT_THAT(t.get(0_uz)) == 42;
  ASSERT_THAT(t.get(1_uz)) == true;

  ASSERT_THAT(t.get(-1_sz)) == true;
  ASSERT_THAT(t.get(-2_sz)) == 42;

  ASSERT_THAT(get<0>(t)) == 42;
  ASSERT_THAT(get<1>(t)) == true;

  template for(constexpr auto v : t.append(42.2, "hello"))
  {
    std::println("{} ({})", v, display_string_of(decay(type_of(^^v))));
  }
  t.invoke([](auto const& a, auto const& b) { std::println("a={}, b={}", a, b); });
  t.append("hello").invoke([](auto const& a, auto const& b, auto const& c)
                           { std::println("a={}, b={}, c={}", a, b, c); });
  t.concat(tuple<int, std::string>{56, "false"})
      .invoke(
          [](auto const&... values)
          {
            (std::print("{}: {}, ", display_string_of(decay(type_of(^^values))), values), ...);
            std::println();
          });

  // structured binding
  {
    auto [a, b] = tuple{1, 2};
  }
  // tie
  {
    int a = -1, b = -1;

    // rvalue tuple
    {
      tie(a, b) = tuple{1, 2};
      ASSERT_THAT(a) == 1;
      ASSERT_THAT(b) == 2;
    }
    // lvalue tuple
    {
      auto t    = tuple{1, 2};
      tie(a, b) = t;
      ASSERT_THAT(a) == 1;
      ASSERT_THAT(b) == 2;
    }
    // const lvalue tuple
    {
      const auto t = tuple{1, 2};
      tie(a, b)    = t;
      ASSERT_THAT(a) == 1;
      ASSERT_THAT(b) == 2;
    }
  }
}
} // namespace tuple_tests
