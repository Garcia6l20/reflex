#pragma once

#include <reflex/const_check.hpp>
#include <reflex/meta.hpp>
#include <reflex/qt/access.hpp>
#include <reflex/qt/detail/annotations.hpp>
#include <reflex/qt/detail/timer.hpp>
#include <reflex/qt/gadget.hpp>

#include <QtCore/qflags.h>

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace reflex::qt::moc
{
namespace detail
{
/** @brief whether @p R names a complete class built on a reflex.qt CRTP base
 *
 * `object<Super, ParentT>` derives `gadget<Super>`, so one test admits both.
 */
consteval bool is_exposable(meta::info R)
{
  return meta::is_type(R) and not meta::is_type_alias(R) and meta::is_class_type(R)
     and meta::is_complete_type(R)
     and meta::is_subclass_of(R, ^^qt::gadget, meta::access_context::unchecked());
}

/** @brief whether @p M is a member reflex.qt publishes to Qt
 *
 * The kinds `meta_strings` collects: annotated member functions, properties,
 * signal and timer data members, nested enumerations and `QFlags` aliases over
 * one of them. Anything here has to be spliceable, which is what
 * @ref check_exposable turns into a diagnostic.
 */
consteval bool is_published(meta::info M)
{
  if(meta::is_type(M))
  {
    if(meta::is_type_alias(M))
    {
      return meta::is_template_instance_of(meta::dealias(M), ^^QFlags);
    }
    return meta::is_enum_type(M);
  }
  if(meta::is_function(M))
  {
    return meta::has_annotation(M, ^^qt::slot_t) or meta::has_annotation(M, ^^qt::invocable_t)
        or not meta::annotations_of_with(M, ^^qt::getter_t).empty()
        or not meta::annotations_of_with(M, ^^qt::setter_t).empty()
        or not meta::annotations_of_with(M, ^^qt::listener_t).empty();
  }
  if(meta::is_nonstatic_data_member(M))
  {
    const auto T = meta::dealias(meta::remove_const(type_of(M)));
    return meta::has_annotation(M, ^^qt::prop)
        or meta::is_template_instance_of(T, ^^qt::detail::signal_decl)
        or meta::is_template_instance_of(T, ^^qt::timer);
  }
  return false;
}

/** @brief every reflex.qt class declared directly in the namespace @p Ns
 *
 * Declaration order, and no recursion: a nested namespace is exposed by naming
 * it, since one is as often an implementation detail as a published group.
 */
consteval auto exposable_types_in(meta::info Ns) -> std::vector<meta::info>
{
  std::vector<meta::info> found;
  for(auto m : meta::members_of(Ns, meta::access_context::unchecked()))
  {
    if(is_exposable(m))
    {
      found.push_back(m);
    }
  }
  return found;
}

/** @brief Rejects a type the metatypes file cannot describe faithfully.
 *
 * @throws std::meta::exception when @p T is not a reflex.qt class, or when one
 *         of its published members cannot be spliced through
 *         `reflex::qt::access<T>`. The second case is an error rather than a
 *         silent omission: a metatypes file describing fewer members than the
 *         `QMetaObject` in the same binary is worse than no file at all.
 */
template <typename T> consteval void check_exposable()
{
  REFLEX_META_CHECK(is_exposable(^^T),
                    std::string{display_string_of(^^T)}
                        + " is not a reflex.qt class; a metatypes entry needs a type deriving "
                          "reflex::qt::gadget<T> or reflex::qt::object<T>",
                    ^^T);

  for(auto m : meta::members_of(^^T, meta::access_context::unchecked()))
  {
    if(is_published(m))
    {
      qt::access<T>::require_reachable(m);
    }
  }
}
} // namespace detail

/** @brief the type list being built, as the body of a REFLEX_QT_MODULE sees it */
struct module_
{
  std::vector<meta::info> types;

  /** @brief publish the reflex.qt class @p T */
  template <typename T> consteval void expose()
  {
    detail::check_exposable<T>();
    if(not std::ranges::contains(types, ^^T))
    {
      types.push_back(^^T);
    }
  }

  /** @brief publish every reflex.qt class declared directly in @p Ns
   *
   * @p Ns may also be a reflection of a class, which publishes that one class.
   */
  template <meta::info Ns> consteval void expose()
  {
    if constexpr(meta::is_namespace(Ns))
    {
      static constexpr auto found = std::define_static_array(detail::exposable_types_in(Ns));
      template for(constexpr auto t : found)
      {
        expose<typename[:t:]>();
      }
    }
    else
    {
      expose<typename[:Ns:]>();
    }
  }
};

/** @brief the classes @p Module exposes, in the order its body named them */
template <typename Module> constexpr auto exposed_types = std::define_static_array(Module::exposed());
} // namespace reflex::qt::moc

/** @brief declare the set of reflex.qt classes a metatypes file describes
 *
 * @code
 * REFLEX_QT_MODULE(app_types, m)
 * {
 *   m.expose<^^app::controllers>();
 *   m.expose<app::settings>();
 * }
 * @endcode
 *
 * `name` becomes a type, and `reflex::qt::moc::write_metatypes<name>(path)`
 * writes the file `qmltyperegistrar` consumes. The body is a consteval
 * function, so the list is a compile-time one: there is no registry, nothing
 * runs before `main`, and a class is described because a module body named it.
 */
#define REFLEX_QT_MODULE(name, m) REFLEX_QT_MODULE_IMPL(name, m)

#define REFLEX_QT_MODULE_IMPL(name, m)                                                             \
  static consteval void reflex_qt_body_##name(::reflex::qt::moc::module_&);                               \
  struct name                                                                                      \
  {                                                                                                \
    static constexpr ::std::string_view module_name = #name;                                       \
    static consteval auto exposed()->::std::vector<::reflex::meta::info>                            \
    {                                                                                              \
      ::reflex::qt::moc::module_ builder;                                                           \
      reflex_qt_body_##name(builder);                                                              \
      return builder.types;                                                                        \
    }                                                                                              \
  };                                                                                               \
  static consteval void reflex_qt_body_##name(::reflex::qt::moc::module_& m)
