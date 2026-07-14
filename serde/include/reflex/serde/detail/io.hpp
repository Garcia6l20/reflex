#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <concepts>
#include <iterator>
#include <ranges>
#include <string>
#include <type_traits>
#endif

REFLEX_EXPORT namespace reflex::serde::detail
{
  // Shared serializer state and entry point. A backend serializer derives from
  // this, declares a `format_name` (and an optional `format_hint`) static member
  // for diagnostics, and adds its own tag_invoke overloads.
  template <typename OutputIt> class serializer_base
  {
    OutputIt out_;

  public:
    serializer_base(OutputIt out) : out_(out)
    {}

    template <typename T>
      requires std::constructible_from<OutputIt, T&>
    serializer_base(T& out) : out_(OutputIt(out))
    {}

    constexpr OutputIt& out()
    {
      return out_;
    }

    // The explicit object parameter makes `self` the derived serializer, so the
    // backend's tag_invoke overloads are found by ADL on `serialize(self, ...)`.
    template <typename Self, typename T> constexpr void dump(this Self&& self, T const& value)
    {
      using backend = std::remove_cvref_t<Self>;
      if constexpr(requires { serialize(self, value); })
      {
        serialize(self, value);
      }
      else if constexpr(requires { backend::format_hint; })
      {
        static_assert(
            false,
            std::string(display_string_of(^^T)) + " is not serializable to "
                + std::string(backend::format_name) + " " + std::string(backend::format_hint));
      }
      else
      {
        static_assert(
            false,
            std::string(display_string_of(^^T)) + " is not serializable to "
                + std::string(backend::format_name));
      }
    }
  };

  // Shared deserializer cursor over a [begin, end) subrange. Character-stream
  // backends (json, csv) derive from this and add their own parsing on top of
  // cursor_/at_end().
  template <std::input_iterator InputIt> class subrange_deserializer
  {
  public:
    using range_cursor = std::ranges::subrange<InputIt, InputIt>;

  protected:
    range_cursor cursor_;

  public:
    bool at_end() const
    {
      return cursor_.empty();
    }

    subrange_deserializer(InputIt begin, InputIt end) : cursor_{begin, end}
    {}

    template <typename T>
      requires requires(T const& v) { v.view(); }
    subrange_deserializer(T const& v) : cursor_{v.view().begin(), v.view().end()}
    {}

    template <typename T>
      requires requires(T const& v) {
        v.begin();
        v.end();
      }
    subrange_deserializer(T const& v) : cursor_{v.begin(), v.end()}
    {}

    template <typename T>
      requires requires(T& v) {
        InputIt{v};
        InputIt{};
      }
    subrange_deserializer(T& v) : cursor_{InputIt{v}, InputIt{}}
    {}
  };
}
