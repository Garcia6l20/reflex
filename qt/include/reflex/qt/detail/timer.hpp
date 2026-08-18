#pragma once

#include <reflex/const_check.hpp>
#include <reflex/meta.hpp>
#include <reflex/qt/access.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace reflex::qt
{
template <typename Super, typename ParentT> class object;

namespace detail
{
/** @brief rejects a timer declaration that does not name a member function */
consteval bool check_timer_handler(meta::info Handler)
{
  REFLEX_META_CHECK(meta::is_function(Handler) and meta::is_class_member(Handler)
                        and not meta::is_static_member(Handler),
                    "a timer names a non-static member function, but "
                        + std::string{display_string_of(Handler)} + " is not one",
                    Handler);
  return true;
}
}

/** @brief the id of one running timer, declared as a data member of the object
 *
 * ```cpp
 * namespace qt = reflex::qt;
 *
 * struct ticker : qt::object<ticker>
 * {
 *   void tick();
 *   qt::timer<^^tick> tick_timer;
 * };
 * ```
 *
 * The storage cannot live in the CRTP base: `object<Super, ParentT>` is
 * instantiated as a base of @p Super, so @p Super is still incomplete there and
 * a table sized from its members is not spellable - `members_of` on it throws
 * *neither complete class type nor namespace*. Declaring one member per timer
 * moves the four bytes to the class that asked for the timer, and a class with
 * no timer pays nothing.
 */
template <meta::info Handler> class timer
{
  consteval
  {
    detail::check_timer_handler(Handler);
  }

public:
  /** @brief the Qt timer id, or `0` when the timer is not running */
  [[nodiscard]] constexpr int id() const noexcept
  {
    return id_;
  }

  [[nodiscard]] constexpr bool isActive() const noexcept
  {
    return id_ != 0;
  }

private:
  template <typename, typename> friend class object;

  int id_ = 0;
};

namespace detail
{
consteval auto timer_type_of(meta::info M) -> meta::info
{
  return meta::dealias(meta::remove_const(type_of(M)));
}

consteval bool is_timer_member(meta::info M)
{
  return meta::is_template_instance_of(timer_type_of(M), ^^timer);
}

/** @brief the member function a timer member drives
 *
 * `template_arguments_of` yields a reflection *of* a value argument, so an
 * `std::meta::info` parameter comes back wrapped one level deep and must be
 * unwrapped before it compares equal to the handler's own reflection.
 */
consteval auto timer_handler_of(meta::info M) -> meta::info
{
  return extract<meta::info>(template_arguments_of(timer_type_of(M))[0]);
}

/** @brief the timer member driving @p handler, or `meta::null`
 *
 * @p Super's own members come first, then its bases depth-first, so a derived
 * class can start and stop a timer a base declares.
 */
consteval auto timer_member_of(meta::info Super, meta::info handler) -> meta::info
{
  for(auto m : meta::nonstatic_data_members_of(Super, meta::access_context::unchecked()))
  {
    if(is_timer_member(m) and timer_handler_of(m) == handler)
    {
      return m;
    }
  }
  for(auto b : bases_of(Super, meta::access_context::unchecked()))
  {
    if(const auto found = timer_member_of(type_of(b), handler); found != meta::null)
    {
      return found;
    }
  }
  return meta::null;
}

/** @brief the timer members @p Super declares itself
 *
 * A base's timers are dispatched by that base's own `timerEvent`, which the
 * derived one delegates to, so this walk is deliberately not recursive.
 */
consteval auto timer_members_of(meta::info Super) -> std::vector<meta::info>
{
  std::vector<meta::info> list;
  for(auto m : meta::nonstatic_data_members_of(Super, meta::access_context::unchecked()))
  {
    if(is_timer_member(m))
    {
      list.push_back(m);
    }
  }
  return list;
}

/** @brief the timer member driving @p handler, rejecting a handler none declares */
consteval auto required_timer_member_of(meta::info Super, meta::info handler) -> meta::info
{
  const auto m = timer_member_of(Super, handler);
  REFLEX_META_CHECK(m != meta::null,
                    "no timer member of " + std::string{identifier_of(Super)} + " declares "
                        + std::string{display_string_of(handler)}
                        + "; add a timer<^^handler> data member",
                    handler);
  return m;
}

/** @brief rejects a timer @p Super's `timerEvent` cannot dispatch */
consteval bool validate_timers(meta::info Super)
{
  const auto members = timer_members_of(Super);
  for(std::size_t i = 0; i < members.size(); ++i)
  {
    const auto handler = timer_handler_of(members[i]);
    const auto owner   = meta::parent_of(handler);

    REFLEX_META_CHECK(
        owner == Super or meta::is_subclass_of(Super, owner, meta::access_context::unchecked()),
        "the timer " + std::string{identifier_of(members[i])} + " names "
            + std::string{display_string_of(handler)}
            + ", a member function of neither its own class nor a base",
        members[i]);

    for(std::size_t j = i + 1; j < members.size(); ++j)
    {
      REFLEX_META_CHECK(timer_handler_of(members[j]) != handler,
                        "the timers " + std::string{identifier_of(members[i])} + " and "
                            + std::string{identifier_of(members[j])} + " both name "
                            + std::string{display_string_of(handler)}
                            + "; declare one timer per handler",
                        members[j]);
    }
  }
  return true;
}

/** @brief rejects every timer of @p Super its `timerEvent` cannot dispatch or splice */
template <typename Super> consteval bool check_timers()
{
  validate_timers(^^Super);
  template for(constexpr auto m : define_static_array(timer_members_of(^^Super)))
  {
    qt::access<Super>::require_reachable(m);
    constexpr auto handler = timer_handler_of(m);
    using handler_owner    = [:meta::parent_of(handler):];
    qt::access<handler_owner>::require_reachable(handler);
  }
  return true;
}
}
}
