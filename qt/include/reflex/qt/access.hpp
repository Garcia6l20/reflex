#pragma once

#include <reflex/const_check.hpp>
#include <reflex/meta.hpp>

#include <meta>
#include <string>
#include <utility>

namespace reflex::qt
{
/** @brief the one type reflex.qt splices @p Super's own members through
 *
 * moc publishes a class's private slots and private properties, so reflex.qt
 * queries @p Super with `access_context::unchecked()` and reaches members that
 * are private to it. Splicing one is still access-checked where the splice is
 * written, and every such splice in the module is written inside this type, so
 * one friend declaration opens all of them:
 *
 * ```cpp
 * struct controller : reflex::qt::object<controller>
 * {
 *   friend reflex::qt::access<controller>;
 *
 * private:
 *   [[= slot]] void onThing(int n);
 *   [[= prop{}]] int count = 0;
 * };
 * ```
 *
 * Without that line a private slot, invocable, accessor, timer handler,
 * property or enumeration is rejected by @ref require_reachable, which names
 * the member and the line to add. A class whose annotated members are all
 * public needs nothing. A base class declares its own line: a member is
 * spliced through the `access` of the class that declares it.
 */
template <typename Super> struct access
{
  access() = delete;

  /** @brief Calls the member function @p M on @p self. */
  template <meta::info M, typename Self, typename... Args>
  static constexpr decltype(auto) call(Self&& self, Args&&... args)
  {
    return self.[:M:](std::forward<Args>(args)...);
  }

  /** @brief The data member @p M of @p self, const-qualified along with it. */
  template <meta::info M, typename Self> static constexpr auto& member(Self& self)
  {
    return self.[:M:];
  }

  /** @brief A pointer to the member @p M. */
  template <meta::info M> static constexpr auto pointer()
  {
    return &[:M:];
  }

  /** @brief Whether @p M can be spliced from here. */
  static consteval bool reachable(meta::info M)
  {
    return std::meta::is_accessible(M, meta::access_context::current());
  }

  /** @brief Rejects @p M when this type cannot splice it.
   *
   * @return `true`, so the call can stand as a constant initializer.
   * @throws std::meta::exception naming @p M and the friend declaration that
   *         makes it reachable.
   */
  static consteval bool require_reachable(meta::info M)
  {
    const std::string owner{identifier_of(meta::parent_of(M))};
    REFLEX_META_CHECK(reachable(M),
                      "reflex.qt cannot reach " + owner + "::" + meta::spelling_of(M)
                          + ": add 'friend reflex::qt::access<" + owner + ">;' to " + owner,
                      M);
    return true;
  }
};
}
