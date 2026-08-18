#pragma once

#include <reflex/constant.hpp>
#include <reflex/meta.hpp>
#include <reflex/qt/detail/version.hpp>

#include <string_view>

namespace reflex::qt
{
template <typename Super> class gadget;

namespace detail
{
template <typename Super> struct gadget_impl;
template <typename Tag, typename Super> struct meta_strings;

/** @brief marks a member function as `Q_INVOKABLE` */
struct invocable
{
};

/** @brief marks a member function as a slot */
struct slot
{
};

/** @brief marks a member function as the handler of the class's timer events */
struct timer_event
{
};

/** @brief marks a data member as a `Q_PROPERTY`
 *
 * @p specs is a set of one-letter flags: `r` readable, `w` writable, `n` the
 * property notifies on change.
 */
struct property
{
  constant_string specs = "rwn";

  constexpr bool readable() const noexcept
  {
    return specs.get().find('r') != std::string_view::npos;
  }

  constexpr bool writable() const noexcept
  {
    return specs.get().find('w') != std::string_view::npos;
  }

  constexpr bool notify() const noexcept
  {
    return specs.get().find('n') != std::string_view::npos;
  }
};
}

/** @brief a `Q_CLASSINFO` entry, annotating the class itself */
struct classinfo
{
  constant_string key;
  constant_string value;
};
}
