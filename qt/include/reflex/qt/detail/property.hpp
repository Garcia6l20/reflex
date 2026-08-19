#pragma once

#include <reflex/const_check.hpp>
#include <reflex/meta.hpp>
#include <reflex/utils.hpp>

#include <algorithm>
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
 * namespace qt = reflex::qt;
 *
 * struct [[= qt::naming::qt_style]] data : qt::object<data>
 * {
 *   [[= qt::prop{}]] int p1 = 0;
 *
 *   int  getP1() const { return p1 * 2; }
 *   void setP1(int value) { p1 = value / 2; }
 *   void onP1Changed() {}
 * };
 * ```
 */
inline constexpr qt_style_t qt_style{};
} // namespace naming

/** @brief marks a data member as a `Q_PROPERTY`
 *
 * The defaults are readable, writable, and notifying through a generated
 * `<name>Changed` signal, which is `Q_PROPERTY(T x MEMBER x NOTIFY xChanged)`
 * plus the signal rather than `MEMBER` alone: moc generates no notify signal
 * for a `MEMBER` property, so `prop{.notify = false}` is the one that reads
 * back like a bare `MEMBER`. Turning a flag off is a designated initializer.
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
struct prop
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

template <meta::info Property> struct getter_t
{
};

/** @brief marks a member function as the reader of @p Property
 *
 * ```cpp
 * [[= prop{}]] int p1 = 0;
 * [[= getter<^^p1>]] int getP1() const { return p1 * 2; }
 * ```
 *
 * The property is reflected rather than named, so a typo is a compile error at
 * the annotation instead of an accessor that silently never runs. The member
 * must already be declared, which is the natural order anyway.
 */
template <meta::info Property> inline constexpr getter_t<Property> getter{};

template <meta::info Property> struct setter_t
{
};

/** @brief marks a member function as the writer of @p Property
 *
 * The setter replaces the whole write, change detection included: a property
 * with one notifies on every `setProperty`, since only the setter knows
 * whether anything changed.
 *
 * ```cpp
 * [[= setter<^^p1>]] void setP1(int value) { p1 = value / 2; }
 * ```
 */
template <meta::info Property> inline constexpr setter_t<Property> setter{};

template <meta::info Property> struct listener_t
{
};

/** @brief marks a member function as the change handler of @p Property
 *
 * Called by `setProperty` after the write and before the notify signal, and
 * not at all when the write changed nothing.
 *
 * ```cpp
 * [[= listener<^^p1>]] void onP1Changed() { std::println("p1 = {}", p1); }
 * ```
 */
template <meta::info Property> inline constexpr listener_t<Property> listener{};

namespace detail
{
/** @brief @p Property's `prop` annotation */
consteval auto property_spec_of(meta::info Property) -> prop
{
  return meta::annotation_value_of_with<prop>(Property);
}

/** @brief the data member of @p Super named @p name and annotated as a property */
consteval auto property_named(meta::info Super, std::string_view name) -> meta::info
{
  for(auto p : meta::nonstatic_data_members_annotated_with(
          Super, ^^prop, meta::access_context::unchecked()))
  {
    if(identifier_of(p) == name)
    {
      return p;
    }
  }
  return meta::null;
}

/** @brief what @p Accessor is called in a diagnostic */
template <meta::info Accessor> consteval auto accessor_noun() -> std::string_view
{
  if constexpr(Accessor == ^^getter_t)
  {
    return "getter";
  }
  else if constexpr(Accessor == ^^setter_t)
  {
    return "setter";
  }
  else if constexpr(Accessor == ^^listener_t)
  {
    return "listener";
  }
  else
  {
    static_assert(false, "not an accessor annotation");
  }
}

/** @brief the property @p fn's @p Accessor annotation reflects, or `meta::null`
 *
 * `template_arguments_of` yields a reflection *of* a value argument, so the
 * `std::meta::info` the annotation carries comes back wrapped one level deep
 * and has to be extracted before it compares equal to the member's own
 * reflection.
 */
template <meta::info Accessor> consteval auto accessor_target_of(meta::info fn) -> meta::info
{
  for(auto a : meta::annotations_of_with(fn, Accessor))
  {
    return extract<meta::info>(template_arguments_of(type_of(a))[0]);
  }
  return meta::null;
}

/** @brief the name convention mode gives @p Accessor for the property @p name */
template <meta::info Accessor>
consteval auto conventional_name_of(std::string_view name) -> std::string
{
  std::string capitalized{name};
  capitalized[0] = char(reflex::to_upper(capitalized[0]));

  if constexpr(Accessor == ^^getter_t)
  {
    return "get" + capitalized;
  }
  else if constexpr(Accessor == ^^setter_t)
  {
    return "set" + capitalized;
  }
  else if constexpr(Accessor == ^^listener_t)
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
template <meta::info Accessor>
consteval auto accessor_for(meta::info Super, meta::info Property) -> meta::info
{
  const std::string_view name  = identifier_of(Property);
  meta::info             found = meta::null;

  for(auto fn :
      meta::member_functions_annotated_with(Super, Accessor, meta::access_context::unchecked()))
  {
    if(accessor_target_of<Accessor>(fn) != Property)
    {
      continue;
    }
    REFLEX_META_CHECK(found == meta::null,
                      "the property " + std::string{name} + " already has a "
                          + std::string{accessor_noun<Accessor>()} + ", "
                          + meta::spelling_of(found) + "; keep one",
                      fn);
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

template <meta::info Accessor> consteval void check_accessors_of(meta::info Super)
{
  std::vector<meta::info> claimed;

  for(auto fn :
      meta::member_functions_annotated_with(Super, Accessor, meta::access_context::unchecked()))
  {
    const std::string noun{accessor_noun<Accessor>()};
    const std::string named = noun + " " + meta::spelling_of(fn);

    const auto p = accessor_target_of<Accessor>(fn);
    REFLEX_META_CHECK(p != meta::null and meta::is_nonstatic_data_member(p)
                          and meta::has_annotation(p, ^^prop) and parent_of(p) == Super,
                      "the " + named + " names " + std::string{display_string_of(p)}
                          + ", which is not a data member of " + std::string{identifier_of(Super)}
                          + " annotated with prop{}",
                      fn);
    REFLEX_META_CHECK(not std::ranges::contains(claimed, p),
                      "the property " + std::string{identifier_of(p)} + " already has a " + noun
                          + "; keep one",
                      fn);
    claimed.push_back(p);

    const auto declared  = dealias(meta::remove_cvref(type_of(p)));
    const auto arguments = parameters_of(fn);

    if constexpr(Accessor == ^^getter_t)
    {
      REFLEX_META_CHECK(arguments.empty(), "the " + named + " must take no argument", fn);
      REFLEX_META_CHECK(dealias(meta::remove_cvref(return_type_of(fn))) == declared,
                        "the " + named + " must return "
                            + std::string{display_string_of(declared)} + ", the type of "
                            + std::string{identifier_of(p)},
                        fn);
    }
    else if constexpr(Accessor == ^^setter_t)
    {
      REFLEX_META_CHECK(
          arguments.size() == 1, "the " + named + " must take exactly one argument", fn);
      REFLEX_META_CHECK(dealias(meta::remove_cvref(type_of(arguments.front()))) == declared,
                        "the " + named + " must take "
                            + std::string{display_string_of(declared)} + ", the type of "
                            + std::string{identifier_of(p)},
                        fn);
    }
    else
    {
      REFLEX_META_CHECK(arguments.empty(), "the " + named + " must take no argument", fn);
      REFLEX_META_CHECK(property_spec_of(p).notify,
                        "the " + named + " needs " + std::string{identifier_of(p)}
                            + " to notify; drop its .notify = false",
                        fn);
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
          Super, ^^prop, meta::access_context::unchecked()))
  {
    const auto spec = property_spec_of(p);
    REFLEX_META_CHECK(spec.read or spec.write,
                      "the property " + std::string{identifier_of(p)}
                          + " is neither readable nor writable; set .read or .write",
                      p);
  }

  check_accessors_of<^^getter_t>(Super);
  check_accessors_of<^^setter_t>(Super);
  check_accessors_of<^^listener_t>(Super);
  return true;
}

/** @brief whether @p Super itself publishes @p Property */
consteval bool declares_property(meta::info Super, meta::info Property)
{
  for(auto p : meta::nonstatic_data_members_annotated_with(
          Super, ^^prop, meta::access_context::unchecked()))
  {
    if(p == Property)
    {
      return true;
    }
  }
  return false;
}

/** @brief rejects a read of something @p Super does not publish as readable */
consteval bool check_readable(meta::info Super, meta::info Property)
{
  REFLEX_META_CHECK(declares_property(Super, Property),
                    std::string{display_string_of(Property)} + " is not a property of "
                        + std::string{identifier_of(Super)} + "; annotate it with prop{}",
                    Property);
  REFLEX_META_CHECK(property_spec_of(Property).read,
                    "the property " + std::string{identifier_of(Property)}
                        + " is declared .read = false and cannot be read",
                    Property);
  return true;
}

/** @brief rejects a write of something @p Super does not publish as writable */
consteval bool check_writable(meta::info Super, meta::info Property)
{
  REFLEX_META_CHECK(declares_property(Super, Property),
                    std::string{display_string_of(Property)} + " is not a property of "
                        + std::string{identifier_of(Super)} + "; annotate it with prop{}",
                    Property);
  REFLEX_META_CHECK(property_spec_of(Property).writable(),
                    "the property " + std::string{identifier_of(Property)}
                        + " is declared .write = false or .constant = true and cannot be written",
                    Property);
  return true;
}

/** @brief rejects a property with no notify signal */
consteval bool check_notifying(meta::info Property)
{
  REFLEX_META_CHECK(property_spec_of(Property).notifying(),
                    "the property " + std::string{identifier_of(Property)}
                        + " publishes no notify signal; drop its .notify = false or "
                          ".constant = true",
                    Property);
  return true;
}

/** @brief the property named @p name in @p Super, rejecting a name it does not declare */
consteval auto required_property_named(meta::info Super, std::string_view name) -> meta::info
{
  const auto p = property_named(Super, name);
  REFLEX_META_CHECK(p != meta::null,
                    std::string{identifier_of(Super)} + " declares no property named "
                        + std::string{name},
                    Super);
  return p;
}

/** @brief the member named @p name in @p Super or a base, rejecting an unknown name */
consteval auto required_member_named(meta::info Super, std::string_view name) -> meta::info
{
  const auto p = meta::member_named(Super, name, meta::access_context::unchecked(), true);
  REFLEX_META_CHECK(p != meta::null,
                    std::string{identifier_of(Super)} + " and its bases declare no member named "
                        + std::string{name},
                    Super);
  return p;
}
} // namespace detail
} // namespace reflex::qt
