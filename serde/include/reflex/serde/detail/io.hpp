#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <algorithm>
#include <charconv>
#include <concepts>
#include <cstddef>
#include <iterator>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
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

  // A container that takes a whole string_view in one call. std::string does.
  template <typename C>
  concept string_bulk_sink_c = requires(C& c) { c.append(std::string_view{}); };

  // A container that takes a whole span of bytes in one call. std::vector<std::byte> does, and it
  // is the natural sink for a binary backend, which has no use for append(string_view).
  template <typename C>
  concept byte_bulk_sink_c =
      requires(C& c) { c.insert_range(c.end(), std::span<std::byte const>{}); };

  // A string destination a reader may fill one character at a time, in the two
  // shapes a reader can fill: growing on demand, or writing into fixed storage
  // through the destination's own iterators.
  //
  // std::string and reflex::heapless::string grow. std::array<char, N> does not
  // and is filled through its iterators, which is what the fixed-capacity path
  // exists for.
  template <typename S>
  concept growable_string_sink_c = requires(S& s) { s.push_back(char{}); };

  template <typename S>
  concept fixed_string_sink_c = requires(S& s) {
    { std::begin(s) } -> std::output_iterator<char>;
    { std::end(s) } -> std::sentinel_for<decltype(std::begin(s))>;
  };

  // Owns storage the reader may write through. The question a reader has to ask
  // before it picks a fill path, and the one a non-owning destination fails: a
  // std::string_view has no push_back and yields a pointer to const, so it
  // satisfies neither half rather than falling into the fixed-capacity one.
  template <typename S>
  concept string_sink_c = growable_string_sink_c<S> or fixed_string_sink_c<S>;

  // The other half: a destination that owns nothing but can be pointed at bytes
  // someone else owns, std::string_view being the one that turns up. Choosing it
  // as the member type is the opt-in to a borrowed read, and to the lifetime
  // contract that comes with one.
  //
  // A backend that offers a borrowed read hands back a view of the input it was
  // given. That view is valid only while that input is alive and unmodified, and
  // nothing in the type system enforces it: deserializing from a temporary
  // std::string leaves every borrowed member dangling. It is spelled out at every
  // site that can produce one.
  //
  // Spelled as construction from a whole run, which is how the character backends
  // have always built a string destination that is not a sink. Any type that used
  // to be built that way still is, so widening the guard does not narrow what
  // compiles: a type that copies the run is simply carried along a path built for
  // one that does not, and copying is safe on either.
  //
  // char const* is deliberately not one of these. A run inside the input carries
  // no terminator.
  template <typename S>
  concept borrowed_string_sink_c =
      (not string_sink_c<S>) and std::constructible_from<S, std::string_view>;

  // A back_insert_iterator exposes its container type but not the container, so
  // the bulk-append fast path needs the container captured at construction. This
  // detects an output iterator whose container can take a whole run at once;
  // everything else (ostreambuf_iterator, plain iterators) falls back to a
  // per-character copy.
  //
  // One specialization with a disjunctive constraint rather than two, so a container satisfying
  // both spellings could never make the choice ambiguous.
  template <typename OutputIt> struct bulk_sink
  {
    using type                      = void;
    static constexpr bool available = false;
  };
  template <typename OutputIt>
    requires string_bulk_sink_c<typename OutputIt::container_type>
          or byte_bulk_sink_c<typename OutputIt::container_type>
  struct bulk_sink<OutputIt>
  {
    using type                      = typename OutputIt::container_type;
    static constexpr bool available = true;
  };

  // Shared serializer state and entry point. A backend serializer derives from
  // this, declares a `format_name` (and an optional `format_hint`) static member
  // for diagnostics, and adds its own tag_invoke overloads.
  template <typename OutputIt> class serializer_base
  {
    OutputIt out_;
    // The container behind out_, when there is one and it can bulk-append.
    // Null when the serializer was handed a bare output iterator.
    typename bulk_sink<OutputIt>::type* sink_ = nullptr;

  public:
    serializer_base(OutputIt out) : out_(out)
    {}

    template <typename T>
      requires std::constructible_from<OutputIt, T&>
    serializer_base(T& out) : out_(OutputIt(out))
    {
      if constexpr(bulk_sink<OutputIt>::available
                   and std::convertible_to<T*, typename bulk_sink<OutputIt>::type*>)
      {
        sink_ = &out;
      }
    }

    constexpr OutputIt& out()
    {
      return out_;
    }

    // The sink container itself, when the serializer was handed one that can take a whole run at
    // once. Null when it was handed a bare output iterator. A backend that can build its output
    // directly in the container uses this to skip the temporary entirely.
    constexpr typename bulk_sink<OutputIt>::type* sink() const
    {
      return sink_;
    }

    // Bulk write. One append when the output is a container that supports it,
    // a per-character copy through the iterator otherwise. Same bytes either
    // way: libstdc++ has no bulk overload of ranges::copy for
    // back_insert_iterator, so without this every byte costs a push_back.
    void write_raw(std::string_view s)
    {
      if constexpr(bulk_sink<OutputIt>::available)
      {
        if constexpr(string_bulk_sink_c<typename bulk_sink<OutputIt>::type>)
        {
          if(sink_ != nullptr)
          {
            sink_->append(s);
            return;
          }
        }
      }
      // Assign the result back: write_char advances out_, so a stateful iterator
      // such as a raw pointer would otherwise have the next write overwrite this one.
      out_ = std::ranges::copy(s, out_).out;
    }

    // The byte-oriented half of write_raw, for binary backends. Same reasoning: without it every
    // byte costs a push_back through the iterator, since libstdc++ has no bulk overload of
    // ranges::copy for back_insert_iterator.
    void write_bytes(std::span<std::byte const> s)
    {
      if constexpr(bulk_sink<OutputIt>::available)
      {
        if constexpr(byte_bulk_sink_c<typename bulk_sink<OutputIt>::type>)
        {
          if(sink_ != nullptr)
          {
            sink_->insert_range(sink_->end(), s);
            return;
          }
        }
      }

      for(std::byte b : s)
      {
        if constexpr(std::output_iterator<OutputIt, std::byte>)
        {
          out_++ = b;
        }
        else if constexpr(std::output_iterator<OutputIt, char>)
        {
          out_++ = static_cast<char>(std::to_integer<unsigned char>(b));
        }
        else
        {
          out_++ = static_cast<unsigned char>(std::to_integer<unsigned char>(b));
        }
      }
    }

    void write_char(char c)
    {
      out_++ = c;
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

  // Offset of the first byte of `set` at or after `pos`, npos if there is none.
  // A min over per-character find() calls, not find_first_of: libstdc++ spells
  // find_first_of as a loop over the input that rescans the needle set for every
  // input byte, while find(char) is memchr.
  //
  // It is a min over N full memchr passes, so it only pays when the span is long
  // and the needle set is small. Callers must pass a bounded `s`, a text node or
  // an attribute value, never the whole remaining input: scanning to EOF for a
  // byte that is not there turns a linear parse quadratic.
  inline std::size_t find_any(std::string_view s, std::size_t pos, std::string_view set)
  {
    std::size_t best = std::string_view::npos;
    for(char c : set)
    {
      const std::size_t n = s.find(c, pos);
      if(n < best)
      {
        best = n;
      }
    }
    return best;
  }

  // Emit `text` with some bytes replaced, in runs.
  //
  // Every text backend needs this and each used to spell it out: json escapes a
  // string body, csv doubles a quote inside a cell, xml replaces three or four
  // bytes with entities and does it twice, once for text and once for an
  // attribute value. Four copies of one loop, differing only in how the next
  // byte of interest is found and what is emitted in its place.
  //
  // `find_next(text, pos)` returns the ABSOLUTE index of the next byte needing
  // replacement at or after `pos`, or npos. `emit_one(ser, c)` writes the
  // replacement for that byte. Everything between is written whole, which is
  // the point: a run goes out in one append rather than a byte at a time.
  //
  // Each caller's find_next must be bounded by what is about to be consumed. A
  // scan for a byte that is not there costs a pass over the remainder, and a
  // caller that rescans from the front on every replacement turns a linear
  // write quadratic. The callers below all advance past what they scanned.
  template <typename Ser, typename Find, typename Emit>
  void write_with_escapes(Ser& ser, std::string_view text, Find&& find_next, Emit&& emit_one)
  {
    std::size_t pos = 0;
    while(pos < text.size())
    {
      const std::size_t n = find_next(text, pos);
      if(n == std::string_view::npos)
      {
        ser.write_raw(text.substr(pos));
        return;
      }
      if(n > pos)
      {
        ser.write_raw(text.substr(pos, n - pos));
      }
      emit_one(ser, text[n]);
      pos = n + 1;
    }
  }

  // A scalar straight into the sink, no intermediate std::string. Two-argument
  // to_chars is shortest-round-trip, which is what "{}" is specified to produce,
  // so the bytes are the same.
  //
  // 64 bytes covers the shortest round-trip form of every type to_chars accepts
  // here, so the result is never truncated.
  template <typename Ser, typename N> void write_digits(Ser& ser, N value)
  {
    char       buf[64];
    const auto r = std::to_chars(buf, buf + sizeof(buf), value);
    ser.write_raw(std::string_view{buf, static_cast<std::size_t>(r.ptr - buf)});
  }

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

    // True when the input is a contiguous block of chars already in memory.
    // Every backend's bulk-scan fast path is gated on this. Public so a caller
    // can static_assert it and find out at compile time that it is on the slow
    // character-at-a-time path.
    static constexpr bool bulk_scan =
        std::contiguous_iterator<InputIt>
        and std::same_as<std::remove_cv_t<std::iter_value_t<InputIt>>, char>;

  protected:
    range_cursor cursor_;
    // Where the input started. cursor_.begin() walks forward and cursor_.end()
    // never moves, so this is the bound that would otherwise be lost, and
    // borrows_input() needs both.
    InputIt first_{};

  public:
    bool at_end() const
    {
      return cursor_.empty();
    }

    // The whole input, consumed part included.
    std::string_view input() const
      requires bulk_scan
    {
      return {std::to_address(first_), static_cast<std::size_t>(cursor_.end() - first_)};
    }

    // True when `s` is a run of the input this deserializer was handed, rather
    // than of a scratch buffer some decoding step owns. This is the question a
    // borrowed read turns on.
    //
    // Asked by address rather than by content on purpose: a decoded run compares
    // equal to what it decoded from often enough that content proves nothing. A
    // backend that parks decoded runs in a pool of its own must still refuse to
    // hand one out, because the pool dies with the deserializer and the
    // deserializer is routinely a temporary that dies before the caller reads the
    // member it filled.
    //
    // An empty run points at nothing and is always safe to hand out.
    bool borrows_input(std::string_view s) const
      requires bulk_scan
    {
      if(s.empty())
      {
        return true;
      }
      const std::string_view in = input();
      return s.data() >= in.data() and s.data() + s.size() <= in.data() + in.size();
    }

    // The unconsumed input. Every scan built on this must be bounded by what it
    // is about to consume: a run that is scanned is always then advanced over,
    // so the parse stays linear. Searching the whole remaining input for a byte
    // that is not there rescans to EOF on every call and makes it quadratic.
    std::string_view rest() const
      requires bulk_scan
    {
      return {std::to_address(cursor_.begin()),
              static_cast<std::size_t>(cursor_.end() - cursor_.begin())};
    }

    // Not constrained on bulk_scan: on a non-contiguous iterator this is n
    // increments, which is exactly what the fallback paths want.
    void skip(std::size_t n)
    {
      cursor_.advance(static_cast<std::ranges::range_difference_t<range_cursor>>(n));
    }

    subrange_deserializer(InputIt begin, InputIt end) : cursor_{begin, end}, first_{begin}
    {}

    // A type exposing both view() and begin()/end() (mmap_input_stream does)
    // would make the two overloads ambiguous. Prefer the range spelling: it is
    // the one that carries the iterator type the cursor is built from.
    template <typename T>
      requires requires(T const& v) { v.view(); } and (not std::ranges::range<T const>)
    subrange_deserializer(T const& v)
        : cursor_{v.view().begin(), v.view().end()}, first_{v.view().begin()}
    {}

    template <typename T>
      requires requires(T const& v) {
        v.begin();
        v.end();
      }
    subrange_deserializer(T const& v) : cursor_{v.begin(), v.end()}, first_{v.begin()}
    {}

    // A char array has no begin(), so the range constructor above does not claim
    // it, and it decays to InputIt, so the stream constructor below did: the
    // cursor was built with a default-constructed end, which for a pointer is
    // null, and every length came out as `nullptr - data`. The array carries its
    // own bound, so use it. One trailing NUL is dropped, which is what a string
    // literal means; an array with no NUL is taken whole rather than read past.
    template <std::size_t N>
      requires std::same_as<InputIt, char const*>
    subrange_deserializer(char const (&v)[N])
        : subrange_deserializer(v, v + (N != 0 and v[N - 1] == '\0' ? N - 1 : N))
    {}

    template <typename T>
      requires requires(T& v) {
        InputIt{v};
        InputIt{};
      }
    subrange_deserializer(T& v) : cursor_{InputIt{v}, InputIt{}}, first_{cursor_.begin()}
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
//
// The contiguous_range guide is what makes serde::mmap_input_stream, or any other
// user type exposing contiguous char iterators, land on the bulk-scan path. The
// basic_string and basic_string_view guides are more specialized so they still win
// for those two, spelling the same iterator type.
//
// The istream guide deduces std::istreambuf_iterator, which is not contiguous, so
// every backend's bulk_scan is false and the parse runs a character at a time. It
// stays for sources that cannot be mapped (a pipe, a socket, std::cin). For a
// file, use serde::mmap_input_stream.
#define REFLEX_SERDE_DESERIALIZER_DEDUCTION_GUIDES(deserializer)                  \
  template <typename... TArgs>                                                    \
  deserializer(std::basic_string<TArgs...> const& in)                             \
      -> deserializer<typename std::basic_string<TArgs...>::const_iterator>;      \
  template <typename... TArgs>                                                    \
  deserializer(std::basic_string_view<TArgs...> const& in)                        \
      -> deserializer<typename std::basic_string_view<TArgs...>::const_iterator>; \
  template <std::ranges::contiguous_range R>                                      \
  deserializer(R const& in) -> deserializer<std::ranges::const_iterator_t<R>>;    \
  template <typename CharT, typename CharTrait = std::char_traits<CharT>>         \
  deserializer(std::basic_istream<CharT, CharTrait>)                              \
      ->deserializer<std::istreambuf_iterator<CharT>>
