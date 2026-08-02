/** @file
 * @brief turning a reflected class into a Python type
 */
#pragma once

#ifndef REFLEX_MODULE
#include <meta>
#include <vector>

#include <reflex/meta.hpp>
#include <reflex/overload_set.hpp>
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
    consteval auto bindable_constructors(std::meta::info T) -> std::vector<reflex::overload>
    {
      std::vector<reflex::overload> kept;
      // The empty name is how a construction is spelled. Going through
      // overloads_of rather than constructors_of is what expands a defaulted
      // parameter into one candidate per reachable arity.
      for(auto o : reflex::overloads_of(T, "", std::meta::access_context::current()))
      {
        if(o.is_aggregate_init())
        {
          // An aggregate has no default constructor for overloads_of to
          // return: its member list covers the same call, and keeping both
          // would be ambiguous. Python has no member-list path, so the empty
          // member list is taken as the default construction and the rest of
          // the list is deferred.
          if(o.arity == 0)
          {
            kept.push_back(o);
          }
          continue;
        }
        const auto ctor = o.function;
        if(std::meta::is_copy_constructor(ctor) or std::meta::is_move_constructor(ctor))
        {
          continue;
        }
        if(std::meta::is_deleted(ctor) or is_skipped(ctor))
        {
          continue;
        }
        if(not std::ranges::all_of(o.parameter_types(), is_bindable_type))
        {
          continue;
        }
        kept.push_back(o);
      }
      return kept;
    }

    consteval auto init_type(reflex::overload o) -> std::meta::info
    {
      return std::meta::substitute(^^nb::init, o.parameter_types());
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

    /** @brief is @p m a member the binder ever looks at
     *
     * The kind has to be settled before anything is asked of the member.
     * members_of also hands back the injected class name, nested types and
     * member templates, and asking one of those for its annotations throws
     * rather than answering.
     *
     * A member function template is among them and it is dropped without a
     * word. It has no parameter types until it is substituted, so there is
     * nothing to bind, and a class not written for Python commonly has one.
     */
    consteval auto is_data(std::meta::info m) -> bool
    {
      return std::meta::is_nonstatic_data_member(m) or std::meta::is_variable(m);
    }

    /** @brief the data members of @p T that become an attribute
     *
     * access_context::current() is evaluated here, outside the class, so a
     * private member is never seen. reflex::overload_set passes unchecked()
     * because it models what the language would do from anywhere; publishing an
     * interface is the opposite question.
     */
    consteval auto bindable_data_members(std::meta::info T) -> std::vector<std::meta::info>
    {
      std::vector<std::meta::info> kept;
      for(auto m : std::meta::members_of(T, std::meta::access_context::current()))
      {
        if(not is_data(m) or is_skipped(m))
        {
          continue;
        }
        if(is_bindable_type(std::meta::type_of(m)))
        {
          kept.push_back(m);
        }
      }
      return kept;
    }

    /** @brief is @p m a member function this binder is willing to publish */
    consteval auto is_bindable_method(std::meta::info m) -> bool
    {
      if(not std::meta::is_function(m) or is_skipped(m))
      {
        return false;
      }
      if(std::meta::is_constructor(m) or std::meta::is_destructor(m)
         or std::meta::is_special_member_function(m))
      {
        return false;
      }
      // An operator has no Python name until the mapping table exists, and
      // "operator+" as an attribute would be unreachable.
      if(std::meta::is_operator_function(m) or std::meta::is_conversion_function(m))
      {
        return false;
      }
      // Python holds the object and calls on it again, so a member that insists
      // on an rvalue has nothing to be called on.
      if(std::meta::is_rvalue_reference_qualified(m))
      {
        return false;
      }
      // The return type settles the whole function. The parameters do not: a
      // defaulted one that cannot be bound only rules out the arity that
      // supplies it, and bindable_overloads decides that per candidate.
      return is_bindable_type(std::meta::return_type_of(m));
    }

    /** @brief one member per distinct method name of @p T, in declaration order
     *
     * A name, not a member, is what an overload set is keyed on, and a name is
     * not structural. The first member carrying it stands in for it: spelling_of
     * on the representative recovers the name, and declaration order is
     * preserved, which is the order nanobind resolves in.
     */
    consteval auto bindable_method_groups(std::meta::info T) -> std::vector<std::meta::info>
    {
      std::vector<std::meta::info> groups;
      for(auto m : std::meta::members_of(T, std::meta::access_context::current()))
      {
        if(is_data(m) or not is_bindable_method(m))
        {
          continue;
        }
        const auto name = meta::spelling_of(m);
        const bool seen = std::ranges::any_of(
            groups, [&](std::meta::info g) { return meta::spelling_of(g) == name; });
        if(not seen)
        {
          groups.push_back(m);
        }
      }
      return groups;
    }

    /** @brief every call to @p name on a @p T that becomes a Python overload
     *
     * reflex::overloads_of does the enumeration, including expanding a defaulted
     * parameter into one candidate per reachable arity. What is added here is
     * what nanobind cannot take: an unbindable signature, and two candidates
     * differing only in the object they want.
     */
    consteval auto bindable_overloads(std::meta::info T, std::string_view name)
        -> std::vector<reflex::overload>
    {
      std::vector<reflex::overload> kept;
      for(auto o : reflex::overloads_of(T, name, std::meta::access_context::current()))
      {
        // A data member holding a callable is reached like a method, which is
        // why overloads_of returns it. It stays a data member here.
        if(o.is_aggregate_init() or o.is_data_member() or not is_bindable_method(o.function))
        {
          continue;
        }
        if(not std::ranges::all_of(o.parameter_types(), is_bindable_type))
        {
          continue;
        }

        // f() and f() const are one name to a Python caller, and nanobind would
        // try them in order and never reach the second. The const one is kept:
        // it is callable on strictly more objects.
        const auto params = o.parameter_types();
        auto       same   = std::ranges::find_if(kept, [&](reflex::overload k) {
          return k.parameter_types() == params;
        });
        if(same == kept.end())
        {
          kept.push_back(o);
        }
        else if(std::meta::is_const(o.function) and not std::meta::is_const(same->function))
        {
          *same = o;
        }
      }
      return kept;
    }

    /** @brief the Python name shared by every overload of @p name on @p T
     *
     * The overloads are one function to a Python caller, so a py::rename on any
     * one of them renames the set. Only an actual rename counts: the others
     * report the name as written, which would otherwise look like a
     * disagreement. Two renames that differ is a mistake worth naming.
     */
    consteval auto group_python_name(std::meta::info T, std::string_view name) -> constant_string
    {
      // A plain string, not a constant_string: constant_string holds a
      // reference to static storage and cannot be reassigned.
      std::string chosen;
      bool        renamed = false;
      for(auto m : meta::callables_named(T, name, std::meta::access_context::current()))
      {
        if(not is_bindable_method(m) or meta::annotations_of_with(m, ^^rename).empty())
        {
          continue;
        }
        const std::string candidate{python_name(m).get()};
        if(not renamed)
        {
          chosen  = candidate;
          renamed = true;
        }
        else if(candidate != chosen)
        {
          throw std::meta::exception(
              "two overloads of the same name ask for different Python names", m);
        }
      }
      if(renamed)
      {
        return constant_string{chosen};
      }
      // No rename anywhere, so the naming policy decides, and every overload
      // reads the same one off the same scope.
      for(auto m : meta::callables_named(T, name, std::meta::access_context::current()))
      {
        if(is_bindable_method(m))
        {
          return python_name(m);
        }
      }
      return constant_string{std::string{name}};
    }

    /** @brief the object type a call to @p fn on a @p T takes
     *
     * A deducing this member spells its object out, so it is read off that
     * parameter. Anything else carries it in its qualifiers.
     */
    consteval auto self_type_of(std::meta::info T, std::meta::info fn) -> std::meta::info
    {
      if(const auto object = meta::explicit_object_type_of(fn); object != meta::null)
      {
        return object;
      }
      const auto self = std::meta::is_const(fn) ? std::meta::add_const(T) : T;
      return std::meta::add_lvalue_reference(self);
    }

    /** @brief must @p o be reached through a generated call rather than a pointer
     *
     * &[:fn:] is the cheaper path and the right one whenever the candidate is
     * called exactly the way it is declared. It is not the universal path: a
     * dropped default argument, a deducing this object, or a reference
     * qualifier each need a call the pointer cannot express.
     */
    consteval auto needs_thunk(reflex::overload o) -> bool
    {
      return o.arity != meta::parameter_types_of(o.function).size()
          or meta::is_explicit_object_member_function(o.function)
          or std::meta::is_lvalue_reference_qualified(o.function);
    }

    /** @brief a call to a member function, with the object as a first parameter
     *
     * A lambda cannot have a parameter pack computed at compile time, and a
     * plain struct with one operator() is accepted by nanobind wherever a lambda
     * is, so this is what a candidate that cannot be a pointer becomes.
     */
    template <std::meta::info Fn, typename Self, typename... Args> struct method_thunk
    {
      decltype(auto) operator()(Self self, Args... args) const
      {
        return (self.[:Fn:])(static_cast<Args&&>(args)...);
      }
    };

    template <std::meta::info Fn, typename... Args> struct static_thunk
    {
      decltype(auto) operator()(Args... args) const
      {
        return [:Fn:](static_cast<Args&&>(args)...);
      }
    };

    consteval auto thunk_type(std::meta::info T, reflex::overload o) -> std::meta::info
    {
      std::vector<std::meta::info> targs{std::meta::reflect_constant(o.function)};
      if(not std::meta::is_static_member(o.function))
      {
        targs.push_back(self_type_of(T, o.function));
      }
      targs.append_range(o.parameter_types());
      return std::meta::substitute(
          std::meta::is_static_member(o.function) ? ^^static_thunk : ^^method_thunk, targs);
    }

    /** @brief the names a Python caller can pass @p o's arguments under
     *
     * All of them or none: nanobind counts the nb::arg annotations against the
     * signature, so a partly named parameter list has to fall back to
     * positional. An unnamed parameter is ordinary C++ and appears in real code.
     *
     * The object is not among them, whether it is spelled out by a deducing this
     * member or not. It is the object, not an argument, and no call site writes
     * it.
     */
    consteval auto argument_names(reflex::overload o) -> std::vector<char const*>
    {
      const auto skip = meta::is_explicit_object_member_function(o.function) ? 1uz : 0uz;

      std::vector<char const*> names;
      for(auto p : std::meta::parameters_of(o.function) | std::views::drop(skip)
                     | std::views::take(o.arity))
      {
        if(not std::meta::has_identifier(p))
        {
          return {};
        }
        names.push_back(std::define_static_string(std::meta::identifier_of(p)));
      }
      return names;
    }

    /** @brief def @p f under @p name, with the argument names and the docstring
     *
     * One nb::arg per real argument and none for the object. class_::def sets
     * nanobind's is_method and it asserts nargs_provided + 1 == nargs, so an
     * nb::arg("self") is a compile error rather than a redundancy. def_static
     * has no is_method and counts every parameter, which comes out the same
     * here because a static member has no object to leave out.
     */
    template <bool Static, reflex::overload O, typename T, typename F>
    void def_candidate(nb::class_<T>& c, char const* name, F f)
    {
      constexpr auto names = std::define_static_array(argument_names(O));

      [&]<std::size_t... I>(std::index_sequence<I...>) {
        // An empty docstring is not passed at all. nanobind would attach it and
        // help() would then show a blank line where the signature belongs.
        if constexpr(not doc_of(O.function).get().empty())
        {
          constexpr auto text = std::define_static_string(doc_of(O.function).get());
          if constexpr(Static)
          {
            c.def_static(name, f, nb::arg(names[I])..., text);
          }
          else
          {
            c.def(name, f, nb::arg(names[I])..., text);
          }
        }
        else if constexpr(Static)
        {
          c.def_static(name, f, nb::arg(names[I])...);
        }
        else
        {
          c.def(name, f, nb::arg(names[I])...);
        }
      }(std::make_index_sequence<names.size()>{});
    }

    template <typename T, reflex::overload O>
    void bind_overload(nb::class_<T>& c, char const* name)
    {
      constexpr bool is_static = std::meta::is_static_member(O.function);

      if constexpr(needs_thunk(O))
      {
        def_candidate<is_static, O>(c, name, typename [:thunk_type(^^T, O):]{});
      }
      else
      {
        def_candidate<is_static, O>(c, name, &[:O.function:]);
      }
    }

    template <typename T> void bind_data_members(nb::class_<T>& c)
    {
      template for(constexpr auto m : std::define_static_array(bindable_data_members(^^T)))
      {
        constexpr auto name = std::define_static_string(python_name(m).get());
        constexpr auto text = std::define_static_string(doc_of(m).get());

        // A static data member splices to an ordinary pointer, which is what
        // the _static forms take.
        constexpr bool instance = std::meta::is_nonstatic_data_member(m);

        if constexpr(not doc_of(m).get().empty())
        {
          if constexpr(instance and is_readonly(m))
          {
            c.def_ro(name, &[:m:], text);
          }
          else if constexpr(instance)
          {
            c.def_rw(name, &[:m:], text);
          }
          else if constexpr(is_readonly(m))
          {
            c.def_ro_static(name, &[:m:], text);
          }
          else
          {
            c.def_rw_static(name, &[:m:], text);
          }
        }
        else if constexpr(instance and is_readonly(m))
        {
          c.def_ro(name, &[:m:]);
        }
        else if constexpr(instance)
        {
          c.def_rw(name, &[:m:]);
        }
        else if constexpr(is_readonly(m))
        {
          c.def_ro_static(name, &[:m:]);
        }
        else
        {
          c.def_rw_static(name, &[:m:]);
        }
      }
    }

    template <typename T> void bind_methods(nb::class_<T>& c)
    {
      template for(constexpr auto group : std::define_static_array(bindable_method_groups(^^T)))
      {
        constexpr auto written = std::define_static_string(meta::spelling_of(group));
        constexpr auto name = std::define_static_string(group_python_name(^^T, written).get());

        template for(constexpr auto o :
                     std::define_static_array(bindable_overloads(^^T, written)))
        {
          bind_overload<T, o>(c, name);
        }
      }
    }
  } // namespace detail

  /** @brief the module being built, as the body of a REFLEX_PY_MODULE sees it */
  struct REFLEX_PY_HIDDEN module_
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

      auto c = [&] {
        if constexpr(not doc_of(^^T).get().empty())
        {
          constexpr auto text = std::define_static_string(doc_of(^^T).get());
          return nb::class_<T>(handle, name ? name : written, text);
        }
        else
        {
          return nb::class_<T>(handle, name ? name : written);
        }
      }();
      detail::bind_constructors(c);
      detail::bind_data_members(c);
      detail::bind_methods(c);
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
