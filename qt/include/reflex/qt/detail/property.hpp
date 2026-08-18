#pragma once

#include <reflex/const_check.hpp>
#include <reflex/constant.hpp>
#include <reflex/meta.hpp>
#include <reflex/utils.hpp>

#include <algorithm>
#include <concepts>
#include <string>
#include <string_view>
#include <vector>

namespace reflex::qt
{
/** @brief accessor naming conventions, annotating the class itself */
namespace naming
{
struct qt_style_t
{};

/** @brief finds a property's accessors by the names Qt code usually gives them
 *
 * A property `p1` takes `getP1` as its getter, `setP1` as its setter and
 * `onP1Changed` as its listener, with no annotation on any of them. An
 * annotated accessor still wins over a conventionally named one.
 *
 * The listener is *not* `p1Changed`: that name is already taken by the notify
 * signal the metaobject publishes for `p1`, and a member function of the same
 * name would read as that signal without being it.
 *
 * ```cpp
 * struct [[= reflex::qt::naming::qt_style]] data : reflex::qt::object<data>
 * {
 *   [[= prop{}]] int p1 = 0;
 *
 *   int  getP1() const { return p1 * 2; }
 *   void setP1(int value) { p1 = value / 2; }
 *   void onP1Changed() {}
 * };
 * ```
 */
inline constexpr qt_style_t qt_style{};
} // namespace naming

namespace detail
{
/** @brief marks a data member as a `Q_PROPERTY`
 *
 * Every flag defaults to what a plain `Q_PROPERTY ... MEMBER` gets from moc:
 * readable, writable, and notifying through a generated `<name>Changed`
 * signal. Turning one off is a designated initializer.
 *
 * ```cpp
 * [[= prop{}]]                  int a;   // read, write, notify
 * [[= prop{.write = false}]]    int b;   // read-only, still notifies
 * [[= prop{.notify = false}]]   int c;   // no cChanged() in the method table
 * [[= prop{.constant = true}]]  int d;   // read-only and silent, both implied
 * ```
 *
 * `constant` implies neither writable nor notifying, which is the only
 * combination Qt accepts: moc rejects `CONSTANT` next to `WRITE` or `NOTIFY`.
 * Since the other flags default to true, an explicit `true` cannot be told
 * apart from the default, so the implication is silent rather than diagnosed.
 * Read the effective values through @ref writable and @ref notifying.
 */
struct property
{
  bool read     = true;
  bool write    = true;
  bool notify   = true;
  bool constant = false;
  bool final    = false;
  bool required = false;

  /** @brief Whether the property accepts a write, `constant` taken into account. */
  constexpr bool writable() const noexcept
  {
    return write and not constant;
  }

  /** @brief Whether the property has a notify signal, `constant` taken into account. */
  constexpr bool notifying() const noexcept
  {
    return notify and not constant;
  }
};

/** @brief marks a member function as the reader of the property it names
 *
 * The property is named rather than reflected, so the annotation does not
 * require the member to be declared first, nor accessible from where the
 * accessor is written.
 *
 * ```cpp
 * [[= getter{"p1"}]] int getP1() const { return p1 * 2; }
 * ```
 */
struct getter : constant_string
{
  using constant_string::constant_string;
};

/** @brief marks a member function as the writer of the property it names
 *
 * The setter replaces the whole write, change detection included: a property
 * with one notifies on every `setProperty`, since only the setter knows
 * whether anything changed.
 *
 * ```cpp
 * [[= setter{"p1"}]] void setP1(int value) { p1 = value / 2; }
 * ```
 */
struct setter : constant_string
{
  using constant_string::constant_string;
};

/** @brief marks a member function as the change handler of the property it names
 *
 * Called by `setProperty` after the write and before the notify signal, and
 * not at all when the write changed nothing.
 *
 * ```cpp
 * [[= listener{"p1"}]] void onP1Changed() { std::println("p1 = {}", p1); }
 * ```
 */
struct listener : constant_string
{
  using constant_string::constant_string;
};

/** @brief @p Property's `property` annotation */
consteval auto property_spec_of(meta::info Property) -> property
{
  return meta::annotation_value_of_with<property>(Property);
}

/** @brief the data member of @p Super named @p name and annotated as a property */
consteval auto property_named(meta::info Super, std::string_view name) -> meta::info
{
  for(auto p : meta::nonstatic_data_members_annotated_with(
          Super, ^^property, meta::access_context::unchecked()))
  {
    if(identifier_of(p) == name)
    {
      return p;
    }
  }
  return meta::null;
}

/** @brief the name convention mode gives @p Accessor for the property @p name */
template <typename Accessor>
consteval auto conventional_name_of(std::string_view name) -> std::string
{
  std::string capitalized{name};
  capitalized[0] = char(reflex::to_upper(capitalized[0]));

  if constexpr(std::same_as<Accessor, getter>)
  {
    return "get" + capitalized;
  }
  else if constexpr(std::same_as<Accessor, setter>)
  {
    return "set" + capitalized;
  }
  else if constexpr(std::same_as<Accessor, listener>)
  {
    return "on" + capitalized + "Changed";
  }
  else
  {
    static_assert(false, "not an accessor annotation");
  }
}

/** @brief the member function of @p Super acting as @p Property's @p Accessor
 *
 * An annotation naming the property wins. Failing that, and only when @p Super
 * carries `naming::qt_style`, the conventionally named member function does.
 * `meta::null` when @p Super has neither.
 */
template <typename Accessor>
consteval auto accessor_for(meta::info Super, meta::info Property) -> meta::info
{
  const std::string_view name  = identifier_of(Property);
  meta::info             found = meta::null;

  for(auto fn :
      meta::member_functions_annotated_with(Super, ^^Accessor, meta::access_context::unchecked()))
  {
    if(*meta::annotation_value_of_with<Accessor>(fn) != name)
    {
      continue;
    }
    REFLEX_META_CHECK(found == meta::null, "two accessors of one kind name one property", fn);
    found = fn;
  }
  if(found != meta::null or not meta::has_annotation(Super, ^^naming::qt_style_t))
  {
    return found;
  }

  const auto candidates = meta::functions_named(
      Super, conventional_name_of<Accessor>(name), meta::access_context::unchecked());
  return candidates.empty() ? meta::null : candidates.front();
}

template <typename Accessor> consteval void check_accessors_of(meta::info Super)
{
  std::vector<std::string> claimed;

  for(auto fn :
      meta::member_functions_annotated_with(Super, ^^Accessor, meta::access_context::unchecked()))
  {
    const std::string_view name = *meta::annotation_value_of_with<Accessor>(fn);
    const auto             p    = property_named(Super, name);
    REFLEX_META_CHECK(p != meta::null, "the accessor names no property of this class", fn);
    REFLEX_META_CHECK(
        not std::ranges::contains(claimed, name), "two accessors of one kind name one property",
        fn);
    claimed.emplace_back(name);

    const auto declared  = dealias(meta::remove_cvref(type_of(p)));
    const auto arguments = parameters_of(fn);

    if constexpr(std::same_as<Accessor, getter>)
    {
      REFLEX_META_CHECK(arguments.empty(), "a getter takes no argument", fn);
      REFLEX_META_CHECK(
          dealias(meta::remove_cvref(return_type_of(fn))) == declared,
          "the getter does not return the property's type", fn);
    }
    else if constexpr(std::same_as<Accessor, setter>)
    {
      REFLEX_META_CHECK(arguments.size() == 1, "a setter takes exactly one argument", fn);
      REFLEX_META_CHECK(
          dealias(meta::remove_cvref(type_of(arguments.front()))) == declared,
          "the setter does not take the property's type", fn);
    }
    else
    {
      REFLEX_META_CHECK(arguments.empty(), "a listener takes no argument", fn);
      REFLEX_META_CHECK(
          property_spec_of(p).notify, "a listener needs a property that notifies", fn);
    }
  }
}

/** @brief rejects a property or an accessor annotation that cannot be honoured
 *
 * Evaluated once per class, when its metaobject is built. A test pins each
 * rejection by calling it directly under `REFLEX_CONSTEVAL_THROWS`.
 */
consteval auto validate_properties(meta::info Super) -> bool
{
  for(auto p : meta::nonstatic_data_members_annotated_with(
          Super, ^^property, meta::access_context::unchecked()))
  {
    const auto spec = property_spec_of(p);
    REFLEX_META_CHECK(spec.read or spec.write, "a property is readable, writable or both", p);
  }

  check_accessors_of<getter>(Super);
  check_accessors_of<setter>(Super);
  check_accessors_of<listener>(Super);
  return true;
}
} // namespace detail
} // namespace reflex::qt
