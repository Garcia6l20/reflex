#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <concepts>
#include <iterator>
#include <optional>
#include <ranges>
#include <string>
#include <type_traits>
#endif

REFLEX_EXPORT namespace reflex::serde::detail
{
  template <typename T> struct field_value
  {
    using type = T;
  };
  template <typename T> struct field_value<std::optional<T>>
  {
    using type = T;
  };

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

  // What load<T>() reads: T, or the backend's default_load_type when T is omitted.
  template <typename Backend, typename T> struct loaded_type
  {
    using type = T;
  };
  template <typename Backend> struct loaded_type<Backend, void>
  {
    using type = typename Backend::default_load_type;
  };

  template <typename Backend, typename T>
  using loaded_type_t = typename loaded_type<Backend, T>::type;

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

    // The explicit object parameter makes `self` the derived deserializer, so the backend's
    // tag_invoke overloads are found by ADL on `deserialize(self, ...)`. A backend declaring a
    // `default_load_type` additionally gets `load()` with no explicit type. The return type is
    // spelled out: deducing it would make the deserialize CPO's constraints depend on themselves.
    template <typename T = void, typename Self>
    loaded_type_t<std::remove_cvref_t<Self>, T> load(this Self&& self)
    {
      return deserialize(self, std::type_identity<loaded_type_t<std::remove_cvref_t<Self>, T>>{});
    }
  };
}

// Deduction guides are neither inherited from subrange_deserializer nor expressible through
// reflection, so each backend stamps the shared set with this macro.
#define REFLEX_SERDE_DESERIALIZER_DEDUCTION_GUIDES(deserializer)                  \
  template <typename... TArgs>                                                    \
  deserializer(std::basic_string<TArgs...> const& in)                             \
      -> deserializer<typename std::basic_string<TArgs...>::const_iterator>;      \
  template <typename... TArgs>                                                    \
  deserializer(std::basic_string_view<TArgs...> const& in)                        \
      -> deserializer<typename std::basic_string_view<TArgs...>::const_iterator>; \
  template <typename CharT, typename CharTrait = std::char_traits<CharT>>         \
  deserializer(std::basic_istream<CharT, CharTrait>)                              \
      ->deserializer<std::istreambuf_iterator<CharT>>
