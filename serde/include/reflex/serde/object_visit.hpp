#pragma once

#include <reflex/serde/annotations.hpp>

namespace reflex::serde
{
template <typename T> struct object_visitor;

template <aggregate_c T> struct object_visitor<T>
{
  static constexpr auto __access_context = std::meta::access_context::current();

  template <typename Fn, decays_to_c<T> Agg>
  static inline constexpr decltype(auto) operator()(Fn&& fn, std::string_view key, Agg&& agg)
  {
    template for(constexpr auto& member : define_static_array(
                     nonstatic_data_members_of(remove_reference(^^T), __access_context)))
    {
      if(identifier_of(member) == key or serialized_name(member) == key)
      {
        return std::forward<Fn>(fn)(std::forward<Agg>(agg).[:member:]);
      }
    }
    throw std::runtime_error(
        std::format("Key '{}' does not match any member of {}", key, identifier_of(decay(^^T))));
  }
};

template <typename T>
concept object_visitable_c = is_complete_type(^^object_visitor<std::decay_t<T>>);

template <object_visitable_c T, typename Fn>
constexpr decltype(auto) object_visit(std::span<std::string_view> keys, T&& value, Fn&& fn)
{
  auto first = keys.front();
  auto rest  = keys.subspan(1);
  if(rest.empty())
  {
    return object_visitor<std::decay_t<T>>{}(std::forward<Fn>(fn), first, std::forward<T>(value));
  }
  else
  {
    return object_visitor<std::decay_t<T>>{}(
        [&]<typename N>(N&& nested) {
          using U = std::decay_t<N>;
          if constexpr(object_visitable_c<U>)
          {
            return object_visit(rest, std::forward<decltype(nested)>(nested), std::forward<Fn>(fn));
          }
          else
          {
            return std::forward<Fn>(fn)(std::forward<decltype(value)>(value));
          }
        },
        first, std::forward<T>(value));
  }
}

template <object_visitable_c T, typename Fn>
constexpr decltype(auto) object_visit(std::string_view key, T&& value, Fn&& fn)
{
  const auto to_sv = [](auto&& r) { return std::string_view(r.begin(), r.end()); };
  auto rng = key | std::views::split('.') | std::views::transform(to_sv);
  std::array<std::string_view, 16> keys{};
  auto const key_count = std::ranges::copy(rng, keys.begin()).out - keys.begin();
  return object_visit(
      std::span(keys.data(), key_count), std::forward<T>(value), std::forward<Fn>(fn));
}

} // namespace reflex::serde
