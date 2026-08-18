#pragma once

#include <reflex/meta.hpp>

#include <vector>

namespace reflex::qt
{
template <typename Super, typename ParentT> class object;

namespace detail
{
/** @brief the id of one running timer, declared as a data member of the object
 *
 * ```cpp
 * struct ticker : reflex::qt::object<ticker>
 * {
 *   void tick();
 *   timer<^^tick> tick_timer;
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
template <meta::info Handler> class timer_decl
{
  static_assert(meta::is_function(Handler) and meta::is_class_member(Handler)
                    and not meta::is_static_member(Handler),
                "a timer must name a non-static member function");

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
  template <typename, typename> friend class qt::object;

  int id_ = 0;
};

consteval auto timer_type_of(meta::info M) -> meta::info
{
  return meta::dealias(meta::remove_const(type_of(M)));
}

consteval bool is_timer_member(meta::info M)
{
  return meta::is_template_instance_of(timer_type_of(M), ^^timer_decl);
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

consteval bool timer_handlers_are_reachable(meta::info Super)
{
  for(auto m : timer_members_of(Super))
  {
    const auto owner = meta::parent_of(timer_handler_of(m));
    if(owner != Super
       and not meta::is_subclass_of(Super, owner, meta::access_context::unchecked()))
    {
      return false;
    }
  }
  return true;
}

consteval bool timer_handlers_are_unique(meta::info Super)
{
  const auto members = timer_members_of(Super);
  for(std::size_t i = 0; i < members.size(); ++i)
  {
    for(std::size_t j = i + 1; j < members.size(); ++j)
    {
      if(timer_handler_of(members[i]) == timer_handler_of(members[j]))
      {
        return false;
      }
    }
  }
  return true;
}
}
}
