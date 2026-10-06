/** @file
 * @brief tp_traverse and tp_clear for a class holding Python references
 *
 * A bound class is invisible to CPython's cycle collector by default. Whatever
 * Python objects its native members hold are references the collector cannot
 * count, so a cycle through one is never found and never broken. py::traverse
 * and py::clear say where those references live, and the binder installs the
 * two type slots that report and drop them.
 */
#pragma once

#ifndef REFLEX_MODULE
#include <concepts>
#include <meta>
#include <ranges>
#include <type_traits>
#include <utility>
#include <vector>

#include <reflex/meta.hpp>
#endif

#include <reflex/const_check.hpp>
#include <reflex/py/annotations.hpp>
#include <reflex/py/nanobind.hpp>

REFLEX_EXPORT namespace reflex::py
{
  namespace detail
  {
    template <typename T, bool Borrowed = false> consteval auto holds_python() -> bool
    {
      if constexpr(std::derived_from<T, nb::object>)
      {
        return true;
      }
      else if constexpr(std::derived_from<T, nb::handle>)
      {
        return Borrowed;
      }
      else if constexpr(requires(T const& t) {
                          t.has_value();
                          *t;
                        })
      {
        return holds_python<std::remove_cvref_t<decltype(*std::declval<T const&>())>, Borrowed>();
      }
      else if constexpr(requires {
                          typename T::first_type;
                          typename T::second_type;
                        })
      {
        return holds_python<std::remove_cv_t<typename T::first_type>, Borrowed>()
            or holds_python<std::remove_cv_t<typename T::second_type>, Borrowed>();
      }
      else if constexpr(std::ranges::input_range<T const>)
      {
        return holds_python<std::remove_cv_t<std::ranges::range_value_t<T const>>, Borrowed>();
      }
      else
      {
        return false;
      }
    }

    template <typename T> inline constexpr bool holds_python_v = holds_python<T>();

    template <typename T> inline constexpr bool holds_handle_v = holds_python<T, true>();
  }

  /** @brief what a py::traverse member function reports the references it owns to
   *
   * A thin wrapper over CPython's `visitproc`. It stops calling it after the
   * first non-zero answer and keeps that answer, so a traverse function visits
   * everything unconditionally and returns nothing.
   */
  class REFLEX_PY_HIDDEN gc_visitor
  {
  public:
    /** @brief wrap the @p visit and @p arg tp_traverse was handed */
    gc_visitor(visitproc visit, void* arg) noexcept : visit_{visit}, arg_{arg}
    {}

    /** @brief report @p held as owned by the object being traversed
     *
     * @p held is an `nb::handle` or anything derived from one, a
     * `std::optional` of one, a range of them, or a pair, a map entry, holding
     * one. A null handle and an empty optional report nothing. Inside a pair,
     * only the side holding a handle is reported, so a map keyed by string
     * works as is.
     *
     * A bare `nb::handle` is reported as given: it is how a reference found
     * through `nb::find` on native state is reported, and it is the caller's
     * word that the object owns it.
     */
    template <typename T> void operator()(T const& held) noexcept
    {
      if constexpr(std::derived_from<T, nb::handle>)
      {
        if(status_ == 0 and held.ptr() != nullptr)
        {
          status_ = visit_(held.ptr(), arg_);
        }
      }
      else if constexpr(requires {
                          held.has_value();
                          *held;
                        })
      {
        if(held.has_value())
        {
          (*this)(*held);
        }
      }
      else if constexpr(requires {
                          held.first;
                          held.second;
                        })
      {
        using first  = std::remove_cvref_t<decltype(held.first)>;
        using second = std::remove_cvref_t<decltype(held.second)>;
        static_assert(
            detail::holds_handle_v<first> or detail::holds_handle_v<second>,
            "neither side of this pair holds an nb::handle");
        if constexpr(detail::holds_handle_v<first>)
        {
          (*this)(held.first);
        }
        if constexpr(detail::holds_handle_v<second>)
        {
          (*this)(held.second);
        }
      }
      else if constexpr(std::ranges::input_range<T const>)
      {
        for(auto const& element : held)
        {
          (*this)(element);
        }
      }
      else
      {
        static_assert(
            false,
            "a py::gc_visitor reports an nb::handle, an optional of one, a range of them "
            "or a pair holding one");
      }
    }

    /** @brief what tp_traverse returns: 0, or the first non-zero visit */
    [[nodiscard]] auto status() const noexcept -> int
    {
      return status_;
    }

  private:
    visitproc visit_;
    void*     arg_;
    int       status_ = 0;
  };

  namespace detail
  {
    /** @brief the members of @p T and of every base carrying @p annotation
     *
     * Bases first, each member once. Private members count: what an object
     * owns has nothing to do with what Python may see of it.
     */
    consteval auto gc_members_of(std::meta::info T, std::meta::info annotation)
        -> std::vector<std::meta::info>
    {
      std::vector<std::meta::info> found;
      const auto keep = [&](std::meta::info m) {
        if(std::ranges::find(found, m) == found.end())
        {
          found.push_back(m);
        }
      };
      for(auto b : std::meta::bases_of(T, std::meta::access_context::unchecked()))
      {
        for(auto m : gc_members_of(std::meta::type_of(b), annotation))
        {
          keep(m);
        }
      }
      for(auto m : std::meta::members_of(T, std::meta::access_context::unchecked()))
      {
        const bool annotatable = std::meta::is_function(m) or std::meta::is_variable(m)
                              or std::meta::is_nonstatic_data_member(m);
        if(not annotatable or not meta::has_annotation(m, annotation))
        {
          continue;
        }
        if(annotation == ^^clear_t)
        {
          REFLEX_META_CHECK(
              std::meta::is_function(m) and not std::meta::is_static_member(m),
              "py::clear goes on a non-static member function",
              m);
        }
        else if(std::meta::is_function(m))
        {
          REFLEX_META_CHECK(
              not std::meta::is_static_member(m),
              "py::traverse goes on a non-static member function",
              m);
        }
        if(std::meta::is_function(m))
        {
          REFLEX_META_CHECK(
              std::meta::is_noexcept(m),
              "a py::traverse or py::clear member function runs inside the collector, so it is "
              "noexcept",
              m);
        }
        else
        {
          REFLEX_META_CHECK(
              std::meta::is_nonstatic_data_member(m),
              "py::traverse goes on a non-static data member or member function",
              m);
          const auto type = std::meta::type_of(m);
          REFLEX_META_CHECK(
              not std::meta::is_const(type) and not std::meta::is_reference_type(type),
              "a py::traverse data member is reset by tp_clear, so it cannot be const or a "
              "reference",
              m);
          REFLEX_META_CHECK(
              std::meta::extract<bool>(
                  std::meta::substitute(^^holds_python_v, {std::meta::remove_cv(type)})),
              "a py::traverse data member holds an nb::object, an optional of one, a range "
              "of them or a map holding them",
              m);
        }
        keep(m);
      }
      return found;
    }

    /** @brief does @p T get tp_traverse and tp_clear */
    consteval auto is_traversed(std::meta::info T) -> bool
    {
      const bool traversed = not gc_members_of(T, ^^traverse_t).empty();
      const bool cleared   = not gc_members_of(T, ^^clear_t).empty();
      REFLEX_META_CHECK(
          traversed or not cleared,
          "a py::clear without a py::traverse is never called",
          T);
      return traversed;
    }

    /** @brief reset @p held without allocating
     *
     * A bare `nb::object` becomes `None`, which costs an incref. Any other
     * `nb::object` subclass becomes a null handle: its default constructor
     * allocates a fresh empty list or dict, which tp_clear must not.
     */
    template <typename T> void drop(T& held) noexcept
    {
      if constexpr(std::same_as<T, nb::object>)
      {
        auto dropped = std::exchange(held, nb::none());
      }
      else if constexpr(std::derived_from<T, nb::object>)
      {
        auto dropped = std::exchange(held, nb::steal<T>(nb::handle{}));
      }
      else
      {
        static_assert(
            std::is_nothrow_default_constructible_v<T>,
            "a py::traverse data member is reset to T{} by tp_clear, which must not throw");
        auto dropped = std::exchange(held, T{});
      }
    }

    /** @brief does @p self own the C++ object it wraps
     *
     * A wrapper made under rv_policy::reference or reference_internal views an
     * object something else owns: a class-typed member through def_rw, or a
     * `T&` return. Whatever that object holds is reported by its owner, if by
     * anyone. Reporting it from a view too explains away a reference the view
     * does not hold, and the collector then clears an object still in use.
     */
    inline auto owns_instance(PyObject* self) noexcept -> bool
    {
      const auto [ready, destruct] = nb::inst_state(nb::handle{self});
      return ready and destruct;
    }

    /** @brief tp_traverse for @p T
     *
     * A hook is called through a pointer to member rather than as
     * `object.[:m:](visitor)`: GCC 16.2.1 access-checks a call through a
     * spliced member function, though not a spliced data member, so a private
     * hook would not compile. P2996 exempts both from access checking.
     */
    template <typename T>
    auto gc_traverse(PyObject* self, visitproc visit, void* arg) noexcept -> int
    {
      Py_VISIT(Py_TYPE(self));
      if(not owns_instance(self))
      {
        return 0;
      }
      T const&   object = *nb::inst_ptr<T>(self);
      gc_visitor visitor{visit, arg};
      template for(constexpr auto m : std::define_static_array(gc_members_of(^^T, ^^traverse_t)))
      {
        if constexpr(std::meta::is_function(m))
        {
          constexpr auto fn = &[:m:];
          static_assert(
              std::is_nothrow_invocable_v<decltype(fn), T const&, gc_visitor&>,
              "a py::traverse member function is noexcept and callable on a const object "
              "with a py::gc_visitor&");
          (object.*fn)(visitor);
        }
        else
        {
          visitor(object.[:m:]);
        }
      }
      return visitor.status();
    }

    /** @brief tp_clear for @p T, a hook called as gc_traverse calls one */
    template <typename T> auto gc_clear(PyObject* self) noexcept -> int
    {
      if(not owns_instance(self))
      {
        return 0;
      }
      T& object = *nb::inst_ptr<T>(self);
      template for(constexpr auto m : std::define_static_array(gc_members_of(^^T, ^^traverse_t)))
      {
        if constexpr(not std::meta::is_function(m))
        {
          drop(object.[:m:]);
        }
      }
      template for(constexpr auto m : std::define_static_array(gc_members_of(^^T, ^^clear_t)))
      {
        constexpr auto fn = &[:m:];
        static_assert(
            std::is_nothrow_invocable_v<decltype(fn), T&>,
            "a py::clear member function is noexcept and callable with no argument");
        (object.*fn)();
      }
      return 0;
    }

    /** @brief the type slots handed to nb::type_slots for @p T
     *
     * nanobind sets Py_TPFLAGS_HAVE_GC when it sees a tp_traverse among them.
     */
    template <typename T> auto gc_slots() -> PyType_Slot const*
    {
      static PyType_Slot const slots[] = {
          {Py_tp_traverse, reinterpret_cast<void*>(&gc_traverse<T>)},
          {Py_tp_clear, reinterpret_cast<void*>(&gc_clear<T>)},
          {0, nullptr},
      };
      return slots;
    }
  }

}
