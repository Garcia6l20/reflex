#pragma once

#include <reflex/meta.hpp>

namespace reflex::testing
{
namespace detail
{
template <meta::info Fixture> struct parametrize_annotation
{
  static constexpr meta::info fixture = Fixture;
};
}; // namespace detail

template <meta::info Fixture> static constexpr detail::parametrize_annotation<Fixture> parametrize;
} // namespace reflex::testing
