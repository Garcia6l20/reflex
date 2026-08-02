/** @file
 * @brief turning a reflected class into a Python type
 */
#pragma once

#ifndef REFLEX_MODULE
#include <meta>
#include <vector>

#include <reflex/meta.hpp>
#endif

#include <reflex/py/nanobind.hpp>
#include <reflex/py/policy.hpp>

REFLEX_EXPORT namespace reflex::py
{
  namespace detail
  {
    /** @brief can nanobind carry a value of type @p t across the boundary
     *
     * Completeness is the whole question. An incomplete type has no caster to
     * instantiate, so a signature mentioning one is not a missing overload but
     * a hard error, and the candidate has to go before it is ever emitted.
     *
     * Whether a complete type has been *registered* with nanobind is not asked
     * here and must not be: registration is a runtime fact, resolved lazily at
     * call time, so binding f(other) before other is bound is legal and works.
     */
    consteval auto is_bindable_type(std::meta::info t) -> bool
    {
      auto bare = std::meta::decay(t);
      while(std::meta::is_pointer_type(bare))
      {
        bare = std::meta::decay(std::meta::remove_pointer(bare));
      }
      return std::meta::is_void_type(bare) or std::meta::is_complete_type(bare);
    }

    consteval auto has_bindable_parameters(std::meta::info fn) -> bool
    {
      return std::ranges::all_of(meta::parameter_types_of(fn), is_bindable_type);
    }

    /** @brief the constructors of @p T that become an __init__
     *
     * The copy and move constructors are left out. nanobind gets copy semantics
     * from the type itself, and binding them would add an __init__(self, other)
     * competing with every single-argument overload.
     *
     * The default constructor stays, unlike in reflex::overloads_of, which drops
     * it for an aggregate because the member list covers the same call. Python
     * has no member-list path, so nb::init<> is the only way to spell T().
     */
    consteval auto bindable_constructors(std::meta::info T) -> std::vector<std::meta::info>
    {
      std::vector<std::meta::info> kept;
      for(auto ctor : meta::constructors_of(T))
      {
        if(std::meta::is_copy_constructor(ctor) or std::meta::is_move_constructor(ctor))
        {
          continue;
        }
        if(std::meta::is_deleted(ctor) or is_skipped(ctor))
        {
          continue;
        }
        if(not has_bindable_parameters(ctor))
        {
          continue;
        }
        kept.push_back(ctor);
      }
      return kept;
    }

    consteval auto init_type(std::meta::info ctor) -> std::meta::info
    {
      return std::meta::substitute(^^nb::init, meta::parameter_types_of(ctor));
    }

    template <typename T> void bind_constructors(nb::class_<T>& c)
    {
      template for(constexpr auto ctor : std::define_static_array(bindable_constructors(^^T)))
      {
        // typename, and braces rather than parentheses: nb::init is a class
        // template, so the splice sits in a type position outside a type-only
        // context.
        c.def(typename [:init_type(ctor):]{});
      }
    }
  } // namespace detail

  /** @brief the module being built, as the body of a REFLEX_PY_MODULE sees it */
  struct module_
  {
    nb::module_ handle;

    /** @brief publish @p T as a Python type
     *
     * @p name overrides the one read off the type, which is what a template
     * instantiation needs: `bind<vec<int>>("vec_int")`.
     */
    template <typename T> auto bind(char const* name = nullptr) -> nb::class_<T>
    {
      constexpr auto written = std::define_static_string(python_name(^^T).get());

      auto c = nb::class_<T>(handle, name ? name : written);
      detail::bind_constructors(c);
      return c;
    }
  };

} // namespace reflex::py

/** @brief define a Python extension module whose body binds reflected types
 *
 * @code
 * REFLEX_PY_MODULE(my_ext, m)
 * {
 *   m.bind<my_class>();
 * }
 * @endcode
 *
 * The one macro here, and it stays thin. NB_MODULE has to produce an extern "C"
 * PyInit_<name>, and no amount of reflection makes a symbol name out of a
 * template argument.
 */
#define REFLEX_PY_MODULE(name, m) REFLEX_PY_MODULE_IMPL(name, m)

#define REFLEX_PY_MODULE_IMPL(name, m)                                                             \
  static void reflex_py_body_##name(::reflex::py::module_&);                                       \
  NB_MODULE(name, reflex_py_handle_##name)                                                         \
  {                                                                                                \
    ::reflex::py::module_ reflex_py_module_##name{reflex_py_handle_##name};                        \
    reflex_py_body_##name(reflex_py_module_##name);                                                \
  }                                                                                                \
  static void reflex_py_body_##name(::reflex::py::module_& m)
