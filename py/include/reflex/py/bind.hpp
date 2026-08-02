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

    /** @brief does a call to @p fn carry only types nanobind can convert */
    consteval auto is_bindable_function(std::meta::info fn) -> bool
    {
      return is_bindable_type(std::meta::return_type_of(fn)) and has_bindable_parameters(fn);
    }

    /** @brief the members of @p T that become an attribute of the Python type
     *
     * access_context::current() is evaluated here, outside the class, so a
     * private member is never seen. reflex::overload_set passes unchecked()
     * because it models what the language would do from anywhere; publishing an
     * interface is the opposite question.
     */
    consteval auto bindable_members(std::meta::info T) -> std::vector<std::meta::info>
    {
      std::vector<std::meta::info> kept;
      for(auto m : std::meta::members_of(T, std::meta::access_context::current()))
      {
        const bool data = std::meta::is_nonstatic_data_member(m) or std::meta::is_variable(m);
        const bool function = std::meta::is_function(m);

        // The kind comes first: members_of also hands back the injected class
        // name, nested types, and member templates, and asking one of those for
        // its annotations throws rather than answering.
        //
        // A member function template is among them, and it is dropped without a
        // word. It has no parameter types until it is substituted, so there is
        // nothing to bind, and a class not written for Python commonly has one.
        // Refusing to bind the class over it would mean annotating every such
        // member.
        if(not data and not function)
        {
          continue;
        }
        if(is_skipped(m))
        {
          continue;
        }
        if(data)
        {
          if(is_bindable_type(std::meta::type_of(m)))
          {
            kept.push_back(m);
          }
          continue;
        }
        if(std::meta::is_constructor(m) or std::meta::is_destructor(m)
           or std::meta::is_special_member_function(m))
        {
          continue;
        }
        // An operator has no Python name until the mapping table exists, and
        // "operator+" as an attribute would be unreachable.
        if(std::meta::is_operator_function(m) or std::meta::is_conversion_function(m))
        {
          continue;
        }
        if(not is_bindable_function(m))
        {
          continue;
        }
        kept.push_back(m);
      }
      return kept;
    }

    template <typename T> void bind_members(nb::class_<T>& c)
    {
      template for(constexpr auto m : std::define_static_array(bindable_members(^^T)))
      {
        constexpr auto name = std::define_static_string(python_name(m).get());

        if constexpr(std::meta::is_nonstatic_data_member(m))
        {
          if constexpr(is_readonly(m))
          {
            c.def_ro(name, &[:m:]);
          }
          else
          {
            c.def_rw(name, &[:m:]);
          }
        }
        else if constexpr(std::meta::is_variable(m))
        {
          // A static data member splices to an ordinary pointer, which is what
          // the _static forms take.
          if constexpr(is_readonly(m))
          {
            c.def_ro_static(name, &[:m:]);
          }
          else
          {
            c.def_rw_static(name, &[:m:]);
          }
        }
        else if constexpr(std::meta::is_static_member(m))
        {
          c.def_static(name, &[:m:]);
        }
        else
        {
          c.def(name, &[:m:]);
        }
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
      detail::bind_members(c);
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
