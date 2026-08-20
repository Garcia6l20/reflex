#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <bit>
#include <charconv>
#include <cstring>
#include <span>

#include <reflex/serde.hpp>
#include <reflex/serde/bson_value.hpp>
#endif

#include <reflex/serde/detail/io.hpp>

namespace reflex::serde::bson::detail
{
using bytes = std::vector<std::byte>;

template <typename T>
concept bson_scalar_c = decays_to_c<T, bson::decimal128> or decays_to_c<T, bson::datetime>;

enum class bson_type : std::uint8_t
{
  eof        = 0x00,
  double_    = 0x01,
  string     = 0x02,
  document   = 0x03,
  array      = 0x04,
  datetime   = 0x09,
  boolean    = 0x08,
  null       = 0x0A,
  int32      = 0x10,
  int64      = 0x12,
  decimal128 = 0x13
};

constexpr void append(bytes& out, std::byte byte)
{
  out.push_back(byte);
}

constexpr void append(bytes& out, bson_type type)
{
  out.push_back(static_cast<std::byte>(type));
}

template <std::integral T> constexpr void append(bytes& out, T value)
{
  using unsigned_t = std::make_unsigned_t<T>;
  auto raw         = static_cast<unsigned_t>(value);

  // An explicit little-endian shift loop, not a bit_cast: the writer is deliberately
  // endian-independent and the reader is the side that is native, see read_as.
  std::array<std::byte, sizeof(T)> buf{};
  for(std::size_t i = 0; i < sizeof(T); ++i)
  {
    buf[i] = static_cast<std::byte>((raw >> (8 * i)) & 0xFFu);
  }
  out.append_range(buf);
}

constexpr void append(bytes& out, double value)
{
  append(out, std::bit_cast<std::uint64_t>(value));
}

constexpr void append(bytes& out, decimal128 value)
{
  const auto raw = std::bit_cast<std::array<std::byte, 16>>(value);
  out.append_range(raw);
}

constexpr void append(bytes& out, std::string_view value, bool include_size = false)
{
  if(include_size)
  {
    append(out, static_cast<std::int32_t>(value.size() + 1));
  }
  // One bulk append of the whole body. as_bytes is a reinterpret_cast underneath, so the
  // constant-evaluated path keeps the byte loop.
  if consteval
  {
    for(char ch : value)
    {
      out.push_back(static_cast<std::byte>(static_cast<unsigned char>(ch)));
    }
  }
  else
  {
    out.append_range(std::as_bytes(std::span{value}));
  }

  out.push_back(std::byte{0x00});
}

template <typename T>
constexpr void write_element(bytes& out, std::string_view key, T const& value);

// Append a document to `out` and backpatch its length prefix in place. The length is
// known once the document closes, and the patch offset stays valid because the buffer
// only ever grows at the end.
template <typename Fn> constexpr void make_document(bytes& out, Fn&& write_elements)
{
  const auto patch = out.size();

  // Reserve space for 4-byte BSON document size.
  append(out, std::int32_t{0});
  std::forward<Fn>(write_elements)(out);
  append(out, std::byte{0x00});

  const auto size = static_cast<std::int32_t>(out.size() - patch);
  for(std::size_t i = 0; i < sizeof(std::int32_t); ++i)
  {
    out[patch + i] = static_cast<std::byte>((size >> (8 * i)) & 0xFF);
  }
}

template <typename T>
constexpr void write_document_value(bytes& out, std::string_view key, T const& value)
{
  append(out, detail::bson_type::document);
  append(out, key);

  make_document(out, [&](bytes& doc) {
    if constexpr(map_c<T>)
    {
      for(auto const& [member_key, member_value] : value)
      {
        write_element(doc, std::string_view(member_key), member_value);
      }
    }
    else
    {
      template for(constexpr auto member : define_static_array(nonstatic_data_members_of(
                       decay(^^decltype(value)), std::meta::access_context::current())))
      {
        constexpr std::string_view member_name = serialized_name(member);
        constexpr bool             omit        = serde::omits_when_empty(member);
        auto const&                member_val  = value.[:member:];
        if constexpr(omit)
        {
          if(serde::is_empty_value(member_val))
          {
            continue;
          }
        }
        reflex::visit([&](auto const& v) { write_element(doc, member_name, v); }, member_val);
      }
    }
  });
}

template <typename T>
constexpr void write_array_value(bytes& out, std::string_view key, T const& value)
{
  append(out, detail::bson_type::array);
  append(out, key);

  make_document(out, [&](bytes& doc) {
    std::size_t idx = 0;
    for(auto const& element : value)
    {
      // BSON array keys are the decimal indices. write_element takes a string_view, so render
      // into a stack buffer rather than constructing a std::string per element.
      char       buf[24];
      const auto res   = std::to_chars(buf, buf + sizeof(buf), idx++);
      const auto index = std::string_view{buf, static_cast<std::size_t>(res.ptr - buf)};
      reflex::visit([&](auto const& v) { write_element(doc, index, v); }, element);
    }
  });
}

template <typename T> constexpr void write_element(bytes& out, std::string_view key, T const& value)
{
  using value_t = std::decay_t<T>;

  if constexpr(std::same_as<value_t, null_t>)
  {
    append(out, detail::bson_type::null);
    append(out, key);
  }
  else if constexpr(optional_c<value_t>)
  {
    if(value.has_value())
    {
      write_element(out, key, *value);
    }
    else
    {
      write_element(out, key, null);
    }
  }
  else if constexpr(std::same_as<value_t, bool>)
  {
    append(out, detail::bson_type::boolean);
    append(out, key);
    append(out, value ? std::byte{0x01} : std::byte{0x00});
  }
  else if constexpr(str_c<value_t>)
  {
    append(out, detail::bson_type::string);
    append(out, key);
    append(out, std::string_view(value), true);
  }
  else if constexpr(std::floating_point<value_t>)
  {
    if constexpr(std::same_as<value_t, decimal128>)
    {
      append(out, detail::bson_type::decimal128);
      append(out, key);
      append(out, value);
    }
    else
    {
      append(out, detail::bson_type::double_);
      append(out, key);
      append(out, static_cast<double>(value));
    }
  }
  else if constexpr(std::integral<value_t> and !std::same_as<value_t, bool>)
  {
    if constexpr(std::same_as<value_t, bson::int32>)
    {
      append(out, detail::bson_type::int32);
      append(out, key);
      append(out, value);
    }
    else if constexpr(std::same_as<value_t, bson::int64>)
    {
      append(out, detail::bson_type::int64);
      append(out, key);
      append(out, value);
    }
    else if(
        (value >= std::numeric_limits<std::int32_t>::min())
        and (value <= std::numeric_limits<std::int32_t>::max()))
    {
      append(out, detail::bson_type::int32);
      append(out, key);
      append(out, static_cast<std::int32_t>(value));
    }
    else
    {
      append(out, detail::bson_type::int64);
      append(out, key);
      append(out, static_cast<std::int64_t>(value));
    }
  }
  else if constexpr(std::same_as<value_t, bson::decimal128>)
  {
    append(out, detail::bson_type::decimal128);
    append(out, key);
    append(out, value);
  }
  else if constexpr(std::same_as<value_t, bson::datetime>)
  {
    append(out, detail::bson_type::datetime);
    append(out, key);
    append(out, value.millis_since_epoch);
  }
  else if constexpr(seq_c<value_t>)
  {
    write_array_value(out, key, value);
  }
  else if constexpr(map_c<value_t> or aggregate_c<value_t>)
  {
    write_document_value(out, key, value);
  }
  else if constexpr(enum_c<value_t>)
  {
    using underlying_t = std::underlying_type_t<value_t>;
    write_element(out, key, static_cast<underlying_t>(value));
  }
  else if constexpr(visitable_c<value_t>)
  {
    reflex::visit([&](auto const& inner) { write_element(out, key, inner); }, value);
  }
  else
  {
    static_assert(false, "Unsupported BSON value type");
  }
}

template <typename T> constexpr void encode_root(bytes& out, T const& value)
{
  using U = std::decay_t<T>;
  if constexpr(map_c<U> or (aggregate_c<U> and !bson_scalar_c<U>))
  {
    make_document(out, [&](bytes& doc) {
      if constexpr(map_c<U>)
      {
        for(auto const& [member_key, member_value] : value)
        {
          write_element(doc, std::string_view(member_key), member_value);
        }
      }
      else
      {
        template for(constexpr auto member : define_static_array(
                         nonstatic_data_members_of(^^U, std::meta::access_context::current())))
        {
          constexpr std::string_view member_name = serialized_name(member);
          constexpr bool             omit        = serde::omits_when_empty(member);
          auto const&                member_val  = value.[:member:];
          if constexpr(omit)
          {
            if(serde::is_empty_value(member_val))
            {
              continue;
            }
          }
          reflex::visit([&](auto const& v) { write_element(doc, member_name, v); }, member_val);
        }
      }
    });
  }
  else if constexpr(reflex::visitable_c<U>)
  {
    reflex::visit([&](auto const& v) { encode_root(out, v); }, value);
  }
  else
  {
    make_document(out, [&](bytes& doc) { write_element(doc, "value", value); });
  }
}

} // namespace reflex::serde::bson::detail

REFLEX_EXPORT namespace reflex::serde::bson
{
  template <typename OutputIt>
  concept bson_output_iterator_c = std::output_iterator<OutputIt, std::byte>
                                or std::output_iterator<OutputIt, char>
                                or std::output_iterator<OutputIt, unsigned char>;

  template <bson_output_iterator_c OutputIt>
  class serializer : public serde::detail::serializer_base<OutputIt>
  {
  public:
    using serde::detail::serializer_base<OutputIt>::serializer_base;

    static constexpr std::string_view format_name = "BSON";
  };

  template <typename... TArgs>
  serializer(std::vector<TArgs...>&)
      -> serializer<std::back_insert_iterator<std::vector<TArgs...>>>;

  template <typename CharT, typename CharTrait = std::char_traits<CharT>>
    requires(sizeof(CharT) == 1)
  serializer(std::basic_ostream<CharT, CharTrait>&)
      -> serializer<std::ostreambuf_iterator<CharT, CharTrait>>;

  template <std::input_iterator It> class deserializer
  {
  public:
    using range_type = std::ranges::subrange<It, It>;

  private:
    struct cursor_t
    {
      // The input can report its remaining length in O(1). True for a string_view, a vector or a
      // mapped file, false for an istreambuf_iterator, which is why every check below is guarded.
      static constexpr bool sized_input = std::copyable<It> and std::sized_sentinel_for<It, It>;

      // The input is a block of bytes already in memory, so a run can be read with one memcpy and
      // a key can be handed out as a view into it instead of being copied.
      static constexpr bool contiguous_byte_or_char =
          std::contiguous_iterator<It>
          and (decays_to_c<std::iter_value_t<It>, std::byte>
               or decays_to_c<std::iter_value_t<It>, char>
               or decays_to_c<std::iter_value_t<It>, unsigned char>);

      // Only the non-contiguous path needs somewhere to put a key it cannot borrow. Conditional so
      // a contiguous cursor does not carry a std::string it never touches.
      struct no_key_buffer
      {};

      range_type  range;
      std::size_t position = 0;

      [[no_unique_address]] std::
          conditional_t<contiguous_byte_or_char, no_key_buffer, std::string> key_buf{};

      constexpr bool at_end() const
      {
        return range.empty();
      }

      // Reject a length read out of the document before anything acts on it. Free on a sized
      // input, a no-op otherwise, where read_byte()'s at_end() check is the only guard available.
      constexpr void require(std::size_t n) const
      {
        if constexpr(sized_input)
        {
          if(n > static_cast<std::size_t>(range.end() - range.begin()))
          {
            throw std::runtime_error("Unexpected end of BSON input");
          }
        }
      }

      constexpr std::byte read_byte()
      {
        if(at_end())
        {
          throw std::runtime_error("Unexpected end of BSON input");
        }
        const auto b = std::bit_cast<std::byte>(*range.begin());
        range.advance(1);
        ++position;
        return b;
      }

      constexpr void advance(std::size_t n)
      {
        require(n);
        range.advance(n);
        position += n;
      }

      // A null-terminated key, borrowed from the input when it is contiguous. The
      // view is a run of the input, so it stays valid as long as the input does,
      // not merely until the next cursor operation. On a non-contiguous input it
      // is a view of key_buf, which the next key overwrites.
      //
      // A key is a BSON cstring: terminated, no length prefix, and no embedded
      // null. A value is a BSON string: length-prefixed, and an embedded null is
      // allowed. read_view below is the value spelling and the two do not share
      // an implementation, because they do not share a shape.
      constexpr std::string_view read_key()
      {
        if constexpr(contiguous_byte_or_char)
        {
          const auto* first = std::to_address(range.begin());
          const auto* last  = std::to_address(range.end());
          const auto* ptr   = first;

          while(ptr != last and std::bit_cast<std::byte>(*ptr) != std::byte{0x00})
          {
            ++ptr;
          }

          if(ptr == last)
          {
            throw std::runtime_error("BSON cstring payload must be null-terminated");
          }

          const auto n = static_cast<std::size_t>(ptr - first);
          advance(n + 1);
          return {reinterpret_cast<char const*>(first), n};
        }
        else
        {
          key_buf.clear();
          while(true)
          {
            const std::byte b = read_byte();
            if(b == std::byte{0x00})
            {
              break;
            }
            key_buf.push_back(static_cast<char>(std::to_integer<unsigned char>(b)));
          }
          return key_buf;
        }
      }

      // Every scalar funnels through here. A contiguous input takes the whole four, eight or
      // sixteen bytes in one memcpy and one advance.
      template <std::size_t N> constexpr auto read_bytes()
      {
        std::array<std::byte, N> bytes{};

        if constexpr(contiguous_byte_or_char)
        {
          require(N);

          const auto* first = std::to_address(range.begin());
          if consteval
          {
            for(std::size_t i = 0; i < N; ++i)
            {
              bytes[i] = std::bit_cast<std::byte>(first[i]);
            }
          }
          else
          {
            std::memcpy(bytes.data(), first, N);
          }
          advance(N);
        }
        else
        {
          for(std::size_t i = 0; i < N; ++i)
          {
            bytes[i] = read_byte();
          }
        }
        return bytes;
      }

      template <typename T> constexpr auto read_as()
      {
        // detail::append writes every numeric type little-endian with an explicit shift loop
        // while this reads it back natively, so the fast path must not compile silently where
        // that asymmetry would be wrong.
        static_assert(
            std::endian::native == std::endian::little,
            "The BSON reader is native-endian while the writer is explicitly little-endian. "
            "Make the reader explicitly little-endian before targeting a big-endian machine.");

        return std::bit_cast<T>(read_bytes<sizeof(T)>());
      }

      template <std::integral T> constexpr T read()
      {
        return read_as<T>();
      }

      template <std::same_as<double> T> constexpr T read()
      {
        return read_as<double>();
      }

      template <std::same_as<bson::decimal128> T> constexpr T read()
      {
        return read_as<bson::decimal128>();
      }

      template <std::same_as<std::string> T, bool has_size = false> constexpr T read()
      {
        std::string out;

        if constexpr(has_size)
        {
          auto size = read<std::int32_t>();
          if(size <= 0)
          {
            throw std::runtime_error("Invalid BSON string length");
          }

          // size counts the payload plus its null terminator. Check both before reading either:
          // the contiguous branch below copies straight off this length without touching
          // read_byte(), so nothing downstream would catch it.
          require(static_cast<std::size_t>(size));

          if constexpr(contiguous_byte_or_char)
          {
            const auto* first = std::to_address(range.begin());
            out.assign(reinterpret_cast<char const*>(first), static_cast<std::size_t>(size - 1));
            advance(static_cast<std::size_t>(size - 1));
          }
          else
          {
            // On an unsized input require() cannot vet the length, so a hostile document must not
            // turn into a multi-gigabyte reservation. Grow from a bounded guess, read_byte() stops
            // at end of input either way.
            constexpr std::size_t max_unchecked_reserve = 64 * 1024;

            auto want = static_cast<std::size_t>(size - 1);
            out.reserve(sized_input ? want : std::min(want, max_unchecked_reserve));

            for(std::int32_t i = 0; i < size - 1; ++i)
            {
              out.push_back(static_cast<char>(std::to_integer<unsigned char>(read_byte())));
            }
          }

          if(read_byte() != std::byte{0x00})
          {
            throw std::runtime_error("BSON string payload must be null-terminated");
          }
        }
        else
        {
          // The owning spelling of read_key(), for callers that want to keep the result.
          out = read_key();
        }
        return out;
      }

      // The payload of a length-prefixed BSON string, as a run of the input.
      //
      // Nothing is decoded on the way: BSON carries a string as its bytes with a
      // count in front, so the payload is already a substring of the input and
      // the borrow is exact by construction. There is no case here that has to
      // fall back to a copy, which is what makes this backend's borrow
      // unconditional where the character backends' is not.
      //
      // `size` counts the payload plus its terminator, so the view is size - 1
      // bytes. The terminator is consumed and checked separately and is never
      // part of the view. An embedded null is legal in a BSON string and survives
      // intact, since the length is what bounds the run rather than a scan.
      constexpr std::string_view read_view()
        requires contiguous_byte_or_char
      {
        const auto size = read<std::int32_t>();
        if(size <= 0)
        {
          throw std::runtime_error("Invalid BSON string length");
        }

        // Both the payload and its terminator, vetted before either is touched:
        // the view below is taken straight off this length and nothing
        // downstream would catch an overrun.
        require(static_cast<std::size_t>(size));

        const auto* first = std::to_address(range.begin());
        advance(static_cast<std::size_t>(size - 1));
        if(read_byte() != std::byte{0x00})
        {
          throw std::runtime_error("BSON string payload must be null-terminated");
        }

        // The one cast, in the one place the payload is read. The input element
        // is std::byte, char or unsigned char, and reading any object's bytes
        // through char const* is what that cast is for. read<std::string> spells
        // its copy the same way.
        return {reinterpret_cast<char const*>(first), static_cast<std::size_t>(size - 1)};
      }

    } cursor_;

  public:
    deserializer(It begin, It end) : cursor_({begin, end})
    {}

    deserializer(std::ranges::input_range auto&& range)
        : cursor_{
              {std::ranges::begin(range), std::ranges::end(range)}
    }
    {}

    template <typename T>
      requires requires(T& v) {
        It{v};
        It{};
      }
    deserializer(T& v) : cursor_{{It{v}, It{}}}
    {}

    bool at_end() const
    {
      return cursor_.at_end();
    }

    std::size_t position() const
    {
      return cursor_.position;
    }

    std::byte read_byte()
    {
      return cursor_.read_byte();
    }

    template <typename T> T read()
    {
      return cursor_.template read<T>();
    }

    template <std::same_as<std::string> T, bool has_size = false> T read()
    {
      return cursor_.template read<T, has_size>();
    }

    template <typename Fn> void read_document(Fn&& fn)
    {
      const auto start = cursor_.position;
      const auto size  = cursor_.template read<std::int32_t>();
      if(size < 5)
      {
        throw std::runtime_error("Invalid BSON document length");
      }

      // The 4 length bytes are already consumed, the rest of the declared document must still fit.
      // Rejects a nested length that overruns the buffer at the point it is read rather than after
      // its elements have been walked.
      cursor_.require(static_cast<std::size_t>(size) - sizeof(std::int32_t));

      const std::size_t end_pos = start + static_cast<std::size_t>(size);

      while(cursor_.position < end_pos)
      {
        const auto type = static_cast<detail::bson_type>(cursor_.read_byte());
        if(type == detail::bson_type::eof)
        {
          if(cursor_.position != end_pos)
          {
            throw std::runtime_error("Unexpected BSON bytes after terminator");
          }
          return;
        }

        // Borrowed from the input buffer when it is contiguous, valid for the duration
        // of the call. Copy it to keep it.
        const std::string_view key = cursor_.read_key();
        std::forward<Fn>(fn)(type, key);
        if(cursor_.position > end_pos)
        {
          throw std::runtime_error("BSON document element exceeds declared size");
        }
      }

      throw std::runtime_error("BSON document missing terminator");
    }

    template <typename T> void read_element(detail::bson_type type, T& value)
    {
      using value_t = std::decay_t<T>;

      if constexpr(std::same_as<value_t, null_t>)
      {
        if(type != detail::bson_type::null)
        {
          throw std::runtime_error("Expected BSON null type");
        }
        value = null;
      }
      else if constexpr(optional_c<value_t>)
      {
        if(type == detail::bson_type::null)
        {
          value.reset();
        }
        else
        {
          read_element(type, value.emplace());
        }
      }
      else if constexpr(std::same_as<value_t, bool>)
      {
        if(type != detail::bson_type::boolean)
        {
          throw std::runtime_error("Expected BSON boolean type");
        }
        value = (cursor_.read_byte() != std::byte{0x00});
      }
      else if constexpr(str_c<value_t>)
      {
        if(type != detail::bson_type::string)
        {
          throw std::runtime_error("Expected BSON string type");
        }
        // Which of the two the destination is, asked before anything is read. It
        // cannot be asked through the assignment: a std::string_view is assignable
        // from a std::string, through the conversion operator, so probing with the
        // assignment accepts a view and then leaves it looking at the local.
        if constexpr(serde::detail::string_sink_c<value_t>)
        {
          auto decoded = cursor_.template read<std::string, true>();
          // Probe with the assignment that is actually performed, so the decoded string
          // is moved into the member rather than copied.
          if constexpr(requires { value = std::move(decoded); })
          {
            value = std::move(decoded);
          }
          else
          {
            static_assert(
                false, "BSON string deserialization requires an assignable owning string target");
          }
        }
        else if constexpr(serde::detail::borrowed_string_sink_c<value_t>)
        {
          // Borrowed read. A std::string_view member is handed a run of the input
          // rather than a copy, and choosing that member type is the opt-in.
          //
          // LIFETIME: the view is valid only while the input this deserializer was
          // given stays alive and unmodified. Nothing in the type system enforces
          // that, so `bson::deserializer{std::vector<std::byte>{...}}.load<T>()`
          // leaves every borrowed member dangling. Deserialize from an lvalue that
          // outlives the result, or from serde::mmap_input_stream.
          //
          // Unlike the character backends there is no value this can refuse. BSON
          // does not escape or decode a string, so every payload on a contiguous
          // input is already a run of it.
          if constexpr(not cursor_t::contiguous_byte_or_char)
          {
            static_assert(
                false,
                std::string(display_string_of(dealias(^^value_t)))
                    + " cannot be a BSON string destination on this cursor: a borrowed read needs"
                      " a contiguous input to point at, and this deserializer reads a byte at a"
                      " time (use std::string, or deserialize from a contiguous input)");
          }
          else
          {
            value = value_t{cursor_.read_view()};
          }
        }
        else
        {
          static_assert(
              false,
              std::string(display_string_of(dealias(^^value_t)))
                  + " cannot be a BSON string destination: it neither owns writable storage nor"
                    " can be pointed at a run of the input (expected std::string,"
                    " reflex::heapless::string<N>, std::array<char, N> or std::string_view)");
        }
      }
      else if constexpr(std::same_as<value_t, bson::int32>)
      {
        if(type != detail::bson_type::int32)
        {
          throw std::runtime_error("Expected BSON int32 type");
        }
        value = cursor_.template read<bson::int32>();
      }
      else if constexpr(std::same_as<value_t, bson::int64>)
      {
        if(type != detail::bson_type::int64)
        {
          throw std::runtime_error("Expected BSON int64 type");
        }
        value = cursor_.template read<bson::int64>();
      }
      else if constexpr(std::same_as<value_t, bson::decimal128>)
      {
        if(type != detail::bson_type::decimal128)
        {
          throw std::runtime_error("Expected BSON decimal128 type");
        }
        value = cursor_.template read<bson::decimal128>();
      }
      else if constexpr(std::same_as<value_t, bson::datetime>)
      {
        if(type != detail::bson_type::datetime)
        {
          throw std::runtime_error("Expected BSON datetime type");
        }
        value.millis_since_epoch = cursor_.template read<std::int64_t>();
      }
      else if constexpr(number_c<value_t>)
      {
        switch(type)
        {
          case detail::bson_type::int32:
            value = static_cast<value_t>(cursor_.template read<std::int32_t>());
            return;
          case detail::bson_type::int64:
            value = static_cast<value_t>(cursor_.template read<std::int64_t>());
            return;
          case detail::bson_type::double_:
            value = static_cast<value_t>(cursor_.template read<double>());
            return;
          default:
            throw std::runtime_error("Expected BSON numeric type");
        }
      }
      else if constexpr(seq_c<value_t>)
      {
        if(type != detail::bson_type::array)
        {
          throw std::runtime_error("Expected BSON array type");
        }
        value.clear();
        read_document([this, &value](detail::bson_type elem_type, std::string_view) {
          typename value_t::value_type elem{};
          read_element(elem_type, elem);
          value.push_back(std::move(elem));
        });
      }
      else if constexpr(map_c<value_t>)
      {
        if(type != detail::bson_type::document)
        {
          throw std::runtime_error("Expected BSON document type");
        }
        value.clear();
        read_document([this, &value](detail::bson_type elem_type, std::string_view key) {
          typename value_t::mapped_type mapped{};
          read_element(elem_type, mapped);
          value[typename value_t::key_type{key}] = std::move(mapped);
        });
      }
      else if constexpr(
          object_visitable_c<value_t>
          and !std::same_as<value_t, bson::value>
          and !detail::bson_scalar_c<value_t>)
      {
        if(type != detail::bson_type::document)
        {
          throw std::runtime_error("Expected BSON document type");
        }
        read_document([this, &value](detail::bson_type elem_type, std::string_view key) {
          object_visit_flat(
              key, value, [this, elem_type](auto& member) { read_element(elem_type, member); });
        });
      }
      else if constexpr(std::same_as<value_t, bson::value>)
      {
        switch(type)
        {
          case detail::bson_type::null:
            value = null;
            return;
          case detail::bson_type::boolean:
            value = (cursor_.read_byte() != std::byte{0x00});
            return;
          case detail::bson_type::int32:
            value = cursor_.template read<bson::int32>();
            return;
          case detail::bson_type::int64:
            value = cursor_.template read<bson::int64>();
            return;
          case detail::bson_type::double_:
            value = cursor_.template read<double>();
            return;
          case detail::bson_type::string:
            value = cursor_.template read<std::string, true>();
            return;
          case detail::bson_type::decimal128:
            value = cursor_.template read<bson::decimal128>();
            return;
          case detail::bson_type::datetime:
            value = bson::datetime{cursor_.template read<std::int64_t>()};
            return;
          case detail::bson_type::document:
          {
            auto& object = value.template emplace<bson::object>();
            object.clear();
            read_document([this, &object](detail::bson_type elem_type, std::string_view key) {
              bson::value nested;
              read_element(elem_type, nested);
              object[std::string{key}] = std::move(nested);
            });
            return;
          }
          case detail::bson_type::array:
          {
            auto& array = value.template emplace<bson::array>();
            array.clear();
            read_document([this, &array](detail::bson_type elem_type, std::string_view) {
              bson::value nested;
              read_element(elem_type, nested);
              array.push_back(std::move(nested));
            });
            return;
          }
          default:
            throw std::runtime_error("Unsupported BSON type tag");
        }
      }
      else if constexpr(requires { value.template emplace<bson::value>(); })
      {
        read_element(type, value.template emplace<bson::value>());
      }
      else if constexpr(enum_c<value_t>)
      {
        using underlying_t = std::underlying_type_t<value_t>;
        read_element(type, reinterpret_cast<underlying_t&>(value));
      }
      else
      {
        static_assert(
            false,
            std::string("Unsupported BSON target type: ") + display_string_of(dealias(^^value_t)));
      }
    }

    // Deserialize the next complete BSON document from the stream as T.
    // Calls the deserialize() CPO which dispatches to tag_invoke overloads.
    template <typename T = bson::value> T load()
    {
      return deserialize(*this, std::type_identity<T>{});
    }
  };

  template <std::input_iterator It> deserializer(It, It) -> deserializer<It>;
  template <std::ranges::input_range R>
  deserializer(R&&) -> deserializer<std::ranges::iterator_t<R>>;

  template <typename CharT, typename CharTrait = std::char_traits<CharT>>
  deserializer(std::basic_istream<CharT, CharTrait>&)
      -> deserializer<std::istreambuf_iterator<CharT>>;

  template <bson_output_iterator_c OutputIt, typename T>
  OutputIt tag_invoke(tag_default_t<serde::serialize>, serializer<OutputIt> & ser, T const& value)
  {
    // When the serializer was handed the byte container itself, encode straight into it,
    // with no temporary in between.
    if constexpr(std::same_as<typename serde::detail::bulk_sink<OutputIt>::type, detail::bytes>)
    {
      if(auto* sink = ser.sink(); sink != nullptr)
      {
        detail::encode_root(*sink, value);
        return ser.out();
      }
    }

    // Otherwise the document has to be built somewhere before it can be handed over, because the
    // length prefixes are backpatched and an output iterator cannot be revisited.
    detail::bytes encoded;
    detail::encode_root(encoded, value);
    ser.write_bytes(encoded);

    return ser.out();
  }

  template <typename It> auto tag_invoke(tag_default_t<serde::deserialize>, deserializer<It> & de)
  {
    return deserialize(de, std::type_identity<bson::value>{});
  }

  template <std::input_iterator It, typename T>
  T tag_invoke(tag_default_t<serde::deserialize>, deserializer<It> & de, std::type_identity<T>)
  {
    using U = std::decay_t<T>;
    T value{};

    if constexpr(std::same_as<U, bson::value>)
    {
      bson::object root;
      de.read_document([&](detail::bson_type type, std::string_view key) {
        bson::value member_value;
        de.read_element(type, member_value);
        root[std::string{key}] = std::move(member_value);
      });

      // Scalars are wrapped as {"value": <scalar>} at the BSON root.
      if(root.size() == 1)
      {
        auto it = root.find("value");
        if(it != root.end())
        {
          value = std::move(it->second);
          return value;
        }
      }

      value = std::move(root);
    }
    else if constexpr(
        map_c<U>
        or (object_visitable_c<U> and !std::same_as<U, bson::value> and !detail::bson_scalar_c<U>))
    {
      de.read_document([&](detail::bson_type type, std::string_view key) {
        if constexpr(map_c<U>)
        {
          typename U::mapped_type mapped{};
          de.read_element(type, mapped);
          value[typename U::key_type{key}] = std::move(mapped);
        }
        else
        {
          object_visit_flat(key, value, [&](auto& member) { de.read_element(type, member); });
        }
      });
    }
    else
    {
      bool has_value = false;
      de.read_document([&](detail::bson_type type, std::string_view key) {
        if(key != "value")
        {
          throw std::runtime_error("Expected wrapped BSON scalar key 'value'");
        }
        if(has_value)
        {
          throw std::runtime_error("Duplicate wrapped BSON scalar key");
        }
        has_value = true;
        de.read_element(type, value);
      });

      if(not has_value)
      {
        throw std::runtime_error("Missing wrapped BSON scalar key");
      }
    }

    return value;
  }
} // namespace reflex::serde::bson

REFLEX_EXPORT namespace reflex::serde::ser
{
  constexpr auto bson = ^^reflex::serde::bson::serializer;
}

REFLEX_EXPORT namespace reflex::serde::de
{
  constexpr auto bson = ^^reflex::serde::bson::deserializer;
}
