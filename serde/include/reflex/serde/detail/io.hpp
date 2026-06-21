#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <concepts>
#include <iterator>
#include <ranges>
#endif

REFLEX_EXPORT namespace reflex::serde::detail
{
  // Shared serializer state: the output iterator and its accessor. Each backend
  // serializer derives from this and adds its own dump() and tag_invoke overloads.
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
