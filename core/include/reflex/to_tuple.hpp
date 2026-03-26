#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#include <reflex/concepts.hpp>
#include <reflex/meta.hpp>

#ifndef REFLEX_MODULE
#include <numeric>
#include <tuple>
#endif

REFLEX_EXPORT namespace reflex
{
  template <aggregate_c T> constexpr auto to_tuple(T const& t)
  {
    constexpr auto ctx = std::meta::access_context::current();

    static constexpr auto members =
        define_static_array(meta::nonstatic_data_members_of_r(^^T, ctx));

    static constexpr auto indices = [] {
      std::array<int, members.size()> indices;
      std::ranges::iota(indices, 0);
      return indices;
    }();

    static constexpr auto [... Is] = indices;
    return std::make_tuple(t.[:members[Is]:]...);
  }
} // namespace reflex
