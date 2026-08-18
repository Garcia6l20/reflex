#pragma once

#include <reflex/constant.hpp>
#include <reflex/meta.hpp>

#include <string_view>
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
 *   timer<"tick"> tick_timer;
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
template <constant_string Handler> class timer_decl
{
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

consteval auto timer_handler_of(meta::info M) -> std::string_view
{
  return *extract<constant_string>(template_arguments_of(timer_type_of(M))[0]);
}

/** @brief the timer member driving @p handler, or `meta::null`
 *
 * @p Super's own members come first, then its bases depth-first, so a derived
 * class can start and stop a timer a base declares.
 */
consteval auto timer_member_of(meta::info Super, std::string_view handler) -> meta::info
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
}
}
