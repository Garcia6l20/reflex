#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <algorithm>
#include <bit>
#include <charconv>
#include <cstring>
#include <limits>
#include <span>
#include <stdexcept>

#include <reflex/serde.hpp>
#include <reflex/serde/bson_value.hpp>
#endif

#include <reflex/serde/detail/encode.hpp>
#include <reflex/serde/detail/io.hpp>

namespace reflex::serde::bson::detail
{
using bytes = std::vector<std::byte>;

template <typename T>
concept bson_extended_c = decays_to_c<T, bson::object_id>
                       or decays_to_c<T, bson::timestamp>
                       or decays_to_c<T, bson::regex>
                       or decays_to_c<T, bson::javascript>
                       or decays_to_c<T, bson::symbol>
                       or decays_to_c<T, bson::db_pointer>
                       or decays_to_c<T, bson::min_key>
                       or decays_to_c<T, bson::max_key>
                       or decays_to_c<T, bson::undefined>
                       or decays_to_c<T, bson::code_w_scope>;

template <typename T>
concept bson_scalar_c = decays_to_c<T, bson::decimal128>
                     or decays_to_c<T, bson::datetime>
                     or decays_to_c<T, bson::binary>
                     or bson_extended_c<T>;

static_assert(bson_scalar_c<bson::binary> and not seq_c<bson::binary>);

static_assert(bson_scalar_c<bson::object_id> and not seq_c<bson::object_id>);
static_assert(bson_scalar_c<bson::timestamp> and not seq_c<bson::timestamp>);
static_assert(bson_scalar_c<bson::regex> and not seq_c<bson::regex>);
static_assert(bson_scalar_c<bson::javascript> and not seq_c<bson::javascript>);
static_assert(bson_scalar_c<bson::symbol> and not seq_c<bson::symbol>);
static_assert(bson_scalar_c<bson::db_pointer> and not seq_c<bson::db_pointer>);
static_assert(bson_scalar_c<bson::min_key> and not seq_c<bson::min_key>);
static_assert(bson_scalar_c<bson::max_key> and not seq_c<bson::max_key>);
static_assert(bson_scalar_c<bson::undefined> and not seq_c<bson::undefined>);
static_assert(bson_scalar_c<bson::code_w_scope> and not seq_c<bson::code_w_scope>);

template <typename T>
concept byte_seq_c =
    seq_c<T> and std::same_as<typename std::remove_cvref_t<T>::value_type, std::byte>;

enum class bson_type : std::uint8_t
{
  eof          = 0x00,
  double_      = 0x01,
  string       = 0x02,
  document     = 0x03,
  array        = 0x04,
  binary       = 0x05,
  undefined    = 0x06,
  object_id    = 0x07,
  boolean      = 0x08,
  datetime     = 0x09,
  null         = 0x0A,
  regex        = 0x0B,
  db_pointer   = 0x0C,
  javascript   = 0x0D,
  symbol       = 0x0E,
  code_w_scope = 0x0F,
  int32        = 0x10,
  timestamp    = 0x11,
  int64        = 0x12,
  decimal128   = 0x13,
  max_key      = 0x7F,
  min_key      = 0xFF
};

inline constexpr std::byte binary_old_subtype{0x02};

struct ext_json_binary_body
{
  std::string                                base64;
  [[= serde::rename{"subType"}]] std::string sub_type;
};

struct ext_json_binary
{
  [[= serde::rename{"$binary"}, = serde::inline_table]] ext_json_binary_body binary;
};

struct ext_json_object_id
{
  [[= serde::rename{"$oid"}]] std::string oid;
};

constexpr std::string sorted_regex_options(std::string_view options)
{
  if(options.contains('\0'))
  {
    throw std::invalid_argument("BSON regex options must not contain a null byte");
  }
  std::string sorted{options};
  std::ranges::sort(sorted);
  const auto [dup, end] = std::ranges::unique(sorted);
  sorted.erase(dup, end);
  if(not std::ranges::all_of(sorted, [](char c) { return std::string_view{"ilmsux"}.contains(c); }))
  {
    throw std::invalid_argument("BSON regex options may only hold the letters i, l, m, s, u, x");
  }
  return sorted;
}

struct ext_json_timestamp_body
{
  std::uint32_t t;
  std::uint32_t i;
};

struct ext_json_timestamp
{
  [[= serde::rename{"$timestamp"}, = serde::inline_table]] ext_json_timestamp_body timestamp;
};

struct ext_json_regex_body
{
  std::string pattern;
  std::string options;
};

struct ext_json_regex
{
  [[= serde::rename{"$regularExpression"}, = serde::inline_table]] ext_json_regex_body regex;
};

struct ext_json_javascript
{
  [[= serde::rename{"$code"}]] std::string code;
};

struct ext_json_code_w_scope
{
  [[= serde::rename{"$code"}]] std::string   code;
  [[= serde::rename{"$scope"}, = serde::inline_table]] bson::object scope;
};

struct ext_json_symbol
{
  [[= serde::rename{"$symbol"}]] std::string symbol;
};

struct ext_json_db_pointer_body
{
  [[= serde::rename{"$ref"}]] std::string       ref;
  [[= serde::rename{"$id"}, = serde::inline_table]] ext_json_object_id id;
};

struct ext_json_db_pointer
{
  [[= serde::rename{"$dbPointer"}, = serde::inline_table]] ext_json_db_pointer_body db_pointer;
};

struct ext_json_min_key
{
  [[= serde::rename{"$minKey"}]] int min_key = 1;
};

struct ext_json_max_key
{
  [[= serde::rename{"$maxKey"}]] int max_key = 1;
};

struct ext_json_undefined
{
  [[= serde::rename{"$undefined"}]] bool undefined = true;
};

constexpr ext_json_object_id ext_json_of(bson::object_id const& value)
{
  return {.oid = serde::detail::hex_encode(value.bytes)};
}

constexpr ext_json_timestamp ext_json_of(bson::timestamp const& value)
{
  return {
      .timestamp = {.t = value.seconds, .i = value.increment}
  };
}

constexpr ext_json_regex ext_json_of(bson::regex const& value)
{
  return {
      .regex = {.pattern = value.pattern, .options = sorted_regex_options(value.options)}
  };
}

constexpr ext_json_javascript ext_json_of(bson::javascript const& value)
{
  return {.code = value.code};
}

constexpr ext_json_symbol ext_json_of(bson::symbol const& value)
{
  return {.symbol = value.name};
}

constexpr ext_json_db_pointer ext_json_of(bson::db_pointer const& value)
{
  return {
      .db_pointer = {.ref = value.ref, .id = ext_json_of(value.id)}
  };
}

constexpr ext_json_min_key ext_json_of(bson::min_key const&)
{
  return {};
}

constexpr ext_json_max_key ext_json_of(bson::max_key const&)
{
  return {};
}

constexpr ext_json_undefined ext_json_of(bson::undefined const&)
{
  return {};
}

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

constexpr void append(bytes& out, bson::object_id const& id)
{
  out.append_range(id.bytes);
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

constexpr void append_cstring(bytes& out, std::string_view value, std::string_view what)
{
  if(value.contains('\0'))
  {
    throw std::invalid_argument(std::format("BSON {} must not contain a null byte", what));
  }
  append(out, value);
}

constexpr void check_scope(std::span<std::byte const> scope)
{
  if(scope.size() < 5 or scope.back() != std::byte{0x00})
  {
    throw std::invalid_argument("BSON code with scope needs an encoded scope document");
  }
  std::uint32_t declared = 0;
  for(std::size_t i = 0; i < sizeof(declared); ++i)
  {
    declared |= std::to_integer<std::uint32_t>(scope[i]) << (8 * i);
  }
  if(declared != scope.size())
  {
    throw std::invalid_argument("BSON code with scope length prefix does not match its scope");
  }
}

constexpr void append(bytes& out, bson::code_w_scope const& value)
{
  const auto& scope = value.scope;
  check_scope(scope);
  const auto total = 2 * sizeof(std::int32_t) + value.code.size() + 1 + scope.size();
  if(total > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
  {
    throw std::length_error("BSON code with scope exceeds the int32 length field");
  }
  append(out, static_cast<std::int32_t>(total));
  append(out, std::string_view(value.code), true);
  out.append_range(scope);
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
  else if constexpr(std::same_as<value_t, bson::binary>)
  {
    append(out, detail::bson_type::binary);
    append(out, key);
    const auto old    = value.subtype == binary_old_subtype;
    const auto prefix = old ? sizeof(std::int32_t) : 0;
    const auto limit  = static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max());
    if(value.data.size() > limit - prefix)
    {
      throw std::length_error("BSON binary payload exceeds the int32 length field");
    }
    append(out, static_cast<std::int32_t>(value.data.size() + prefix));
    append(out, value.subtype);
    if(old)
    {
      append(out, static_cast<std::int32_t>(value.data.size()));
    }
    out.append_range(value.data);
  }
  else if constexpr(std::same_as<value_t, bson::object_id>)
  {
    append(out, detail::bson_type::object_id);
    append(out, key);
    append(out, value);
  }
  else if constexpr(std::same_as<value_t, bson::timestamp>)
  {
    append(out, detail::bson_type::timestamp);
    append(out, key);
    append(out, value.increment);
    append(out, value.seconds);
  }
  else if constexpr(std::same_as<value_t, bson::regex>)
  {
    append(out, detail::bson_type::regex);
    append(out, key);
    append_cstring(out, value.pattern, "regex pattern");
    append_cstring(out, sorted_regex_options(value.options), "regex options");
  }
  else if constexpr(std::same_as<value_t, bson::javascript>)
  {
    append(out, detail::bson_type::javascript);
    append(out, key);
    append(out, std::string_view(value.code), true);
  }
  else if constexpr(std::same_as<value_t, bson::symbol>)
  {
    append(out, detail::bson_type::symbol);
    append(out, key);
    append(out, std::string_view(value.name), true);
  }
  else if constexpr(std::same_as<value_t, bson::db_pointer>)
  {
    append(out, detail::bson_type::db_pointer);
    append(out, key);
    append(out, std::string_view(value.ref), true);
    append(out, value.id);
  }
  else if constexpr(std::same_as<value_t, bson::min_key>)
  {
    append(out, detail::bson_type::min_key);
    append(out, key);
  }
  else if constexpr(std::same_as<value_t, bson::max_key>)
  {
    append(out, detail::bson_type::max_key);
    append(out, key);
  }
  else if constexpr(std::same_as<value_t, bson::undefined>)
  {
    append(out, detail::bson_type::undefined);
    append(out, key);
  }
  else if constexpr(std::same_as<value_t, bson::code_w_scope>)
  {
    append(out, detail::bson_type::code_w_scope);
    append(out, key);
    append(out, value);
  }
  else if constexpr(detail::byte_seq_c<value_t>)
  {
    append(out, detail::bson_type::binary);
    append(out, key);
    if(value.size() > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
    {
      throw std::length_error("BSON binary payload exceeds the int32 length field");
    }
    append(out, static_cast<std::int32_t>(value.size()));
    append(out, std::byte{0x00});
    out.append_range(value);
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
  if constexpr(map_c<U> or (aggregate_c<U> and !bson_scalar_c<U> and !seq_c<U>))
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

  namespace detail
  {
    struct bson_access;
  }

  /** @brief Input options of bson::deserializer, its second template argument.
   *
   * With `unknown_keys::skip`, an element whose key matches no member is consumed by its type and
   * length, every BSON element type included. A skipped document or array is checked by its
   * declared length and trailing 0x00 only, a corrupt subtree under a skipped key is not reported.
   */
  struct read_options
  {
    serde::unknown_keys unknown_keys = serde::unknown_keys::error;

    constexpr bool operator==(read_options const&) const = default;
  };

  template <std::input_iterator It, read_options Opts = read_options{}> class deserializer
  {
  public:
    static constexpr read_options read_opts = Opts;

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

      using key_buffer = typename[:contiguous_byte_or_char ? ^^no_key_buffer : ^^std::string:];

      range_type  range;
      std::size_t position = 0;

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

      constexpr void skip(std::size_t n)
      {
        if constexpr(sized_input)
        {
          advance(n);
        }
        else
        {
          for(std::size_t i = 0; i < n; ++i)
          {
            read_byte();
          }
        }
      }

      constexpr void skip_cstring()
      {
        while(read_byte() != std::byte{0x00})
        {}
      }

      // A null-terminated key, borrowed from the input when it is contiguous. The
      // view is a run of the input, so it stays valid as long as the input does,
      // not merely until the next cursor operation. On a non-contiguous input it
      // is a view of the caller's key_buf, which read_document owns per document:
      // a nested document reads its keys into its own buffer, so this key stays
      // valid across the value's read, until the next key of the same document.
      //
      // A key is a BSON cstring: terminated, no length prefix, and no embedded
      // null. A value is a BSON string: length-prefixed, and an embedded null is
      // allowed. read_view below is the value spelling and the two do not share
      // an implementation, because they do not share a shape.
      constexpr std::string_view read_key([[maybe_unused]] key_buffer& key_buf)
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

      constexpr void read_bytes_into(std::byte* dest, std::size_t n)
      {
        if constexpr(contiguous_byte_or_char)
        {
          require(n);

          const auto* first = std::to_address(range.begin());
          if consteval
          {
            for(std::size_t i = 0; i < n; ++i)
            {
              dest[i] = std::bit_cast<std::byte>(first[i]);
            }
          }
          else
          {
            if(n == 0)
            {
              return;
            }
            std::memcpy(dest, first, n);
          }
          advance(n);
        }
        else
        {
          for(std::size_t i = 0; i < n; ++i)
          {
            dest[i] = read_byte();
          }
        }
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
        else if constexpr(contiguous_byte_or_char)
        {
          // The owning spelling of read_key(), for callers that want to keep the result.
          key_buffer none;
          out = read_key(none);
        }
        else
        {
          for(auto b = read_byte(); b != std::byte{0x00}; b = read_byte())
          {
            out.push_back(static_cast<char>(std::to_integer<unsigned char>(b)));
          }
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

    template <auto X>
      requires(std::constant_wrapper<X>::value == Opts)
    deserializer(It begin, It end, std::constant_wrapper<X>) : cursor_({begin, end})
    {}

    template <auto X>
      requires(std::constant_wrapper<X>::value == Opts)
    deserializer(std::ranges::input_range auto&& range, std::constant_wrapper<X>)
        : cursor_{
              {std::ranges::begin(range), std::ranges::end(range)}
    }
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

    template <typename T, auto X>
      requires requires(T& v) {
        It{v};
        It{};
      } and (std::constant_wrapper<X>::value == Opts)
    deserializer(T& v, std::constant_wrapper<X>) : cursor_{{It{v}, It{}}}
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

      typename cursor_t::key_buffer key_buf;
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
        const std::string_view key = cursor_.read_key(key_buf);
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
      else if constexpr(std::same_as<value_t, bson::binary>)
      {
        read_binary(type, value);
      }
      else if constexpr(detail::bson_extended_c<value_t>)
      {
        read_extended(type, value);
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
      else if constexpr(detail::byte_seq_c<value_t>)
      {
        const auto [n, subtype] = binary_header(type);
        if(subtype != std::byte{0x00})
        {
          throw std::runtime_error(std::format(
              "Unsupported BSON binary subtype: {:#04x}", std::to_integer<unsigned>(subtype)));
        }
        if constexpr(requires { value.resize(n); })
        {
          value.clear();
          read_payload(value, n);
        }
        else
        {
          if(n > value.size())
          {
            throw std::out_of_range("Array has more elements than target type can hold");
          }
          cursor_.read_bytes_into(value.data(), n);
          std::ranges::fill(std::span{value}.subspan(n), std::byte{0});
        }
      }
      else if constexpr(seq_c<value_t>)
      {
        if(type != detail::bson_type::array)
        {
          throw std::runtime_error("Expected BSON array type");
        }
        if constexpr(requires { value.clear(); })
        {
          value.clear();
        }
        auto push =
            serde::detail::make_pusher(value, "Array has more elements than target type can hold");
        read_document([this, &push](detail::bson_type elem_type, std::string_view) {
          typename value_t::value_type elem{};
          read_element(elem_type, elem);
          push(std::move(elem));
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
          read_member_or_miss(elem_type, key, value);
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
          case detail::bson_type::binary:
            read_element(type, value.template emplace<bson::binary>());
            return;
          case detail::bson_type::object_id:
            read_extended(type, value.template emplace<bson::object_id>());
            return;
          case detail::bson_type::timestamp:
            read_extended(type, value.template emplace<bson::timestamp>());
            return;
          case detail::bson_type::regex:
            read_extended(type, value.template emplace<bson::regex>());
            return;
          case detail::bson_type::javascript:
            read_extended(type, value.template emplace<bson::javascript>());
            return;
          case detail::bson_type::symbol:
            read_extended(type, value.template emplace<bson::symbol>());
            return;
          case detail::bson_type::db_pointer:
            read_extended(type, value.template emplace<bson::db_pointer>());
            return;
          case detail::bson_type::min_key:
            value = bson::min_key{};
            return;
          case detail::bson_type::max_key:
            value = bson::max_key{};
            return;
          case detail::bson_type::undefined:
            value = bson::undefined{};
            return;
          case detail::bson_type::code_w_scope:
            read_extended(type, value.template emplace<bson::code_w_scope>());
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

  private:
    friend struct detail::bson_access;

    static void expect(detail::bson_type type, detail::bson_type wanted, std::string_view name)
    {
      if(type != wanted)
      {
        throw std::runtime_error(std::format("Expected BSON {} type", name));
      }
    }

    template <typename T> void read_extended(detail::bson_type type, T& value)
    {
      if constexpr(std::same_as<T, bson::object_id>)
      {
        expect(type, detail::bson_type::object_id, "ObjectId");
        value.bytes = cursor_.template read_bytes<12>();
      }
      else if constexpr(std::same_as<T, bson::timestamp>)
      {
        expect(type, detail::bson_type::timestamp, "timestamp");
        value.increment = cursor_.template read<std::uint32_t>();
        value.seconds   = cursor_.template read<std::uint32_t>();
      }
      else if constexpr(std::same_as<T, bson::regex>)
      {
        expect(type, detail::bson_type::regex, "regex");
        value.pattern = cursor_.template read<std::string>();
        value.options = cursor_.template read<std::string>();
      }
      else if constexpr(std::same_as<T, bson::javascript>)
      {
        expect(type, detail::bson_type::javascript, "JavaScript code");
        value.code = cursor_.template read<std::string, true>();
      }
      else if constexpr(std::same_as<T, bson::symbol>)
      {
        expect(type, detail::bson_type::symbol, "symbol");
        value.name = cursor_.template read<std::string, true>();
      }
      else if constexpr(std::same_as<T, bson::db_pointer>)
      {
        expect(type, detail::bson_type::db_pointer, "DBPointer");
        value.ref      = cursor_.template read<std::string, true>();
        value.id.bytes = cursor_.template read_bytes<12>();
      }
      else if constexpr(std::same_as<T, bson::min_key>)
      {
        expect(type, detail::bson_type::min_key, "MinKey");
      }
      else if constexpr(std::same_as<T, bson::max_key>)
      {
        expect(type, detail::bson_type::max_key, "MaxKey");
      }
      else if constexpr(std::same_as<T, bson::code_w_scope>)
      {
        expect(type, detail::bson_type::code_w_scope, "code with scope");
        read_code_w_scope(value);
      }
      else
      {
        expect(type, detail::bson_type::undefined, "undefined");
      }
    }

    std::size_t checked_length(std::int32_t size, std::size_t limit)
    {
      const auto n = static_cast<std::size_t>(size);
      if(cursor_.position > limit or n > limit - cursor_.position)
      {
        throw std::runtime_error("BSON element exceeds declared size");
      }
      cursor_.require(n);
      return n;
    }

    std::size_t string_length(std::size_t limit)
    {
      const auto size = cursor_.template read<std::int32_t>();
      if(size < 1)
      {
        throw std::runtime_error("Invalid BSON string length");
      }
      return checked_length(size, limit);
    }

    std::size_t document_body_length(std::size_t limit)
    {
      const auto size = cursor_.template read<std::int32_t>();
      if(size < 5)
      {
        throw std::runtime_error("Invalid BSON document length");
      }
      return checked_length(size - static_cast<std::int32_t>(sizeof(std::int32_t)), limit);
    }

    std::size_t code_w_scope_limit()
    {
      const auto size = cursor_.template read<std::int32_t>();
      if(size < 14)
      {
        throw std::runtime_error("Invalid BSON code with scope length");
      }
      const auto n = static_cast<std::size_t>(size) - sizeof(std::int32_t);
      cursor_.require(n);
      return cursor_.position + n;
    }

    void end_code_w_scope(std::size_t limit) const
    {
      if(cursor_.position != limit)
      {
        throw std::runtime_error("BSON code with scope length does not match its content");
      }
    }

    void expect_string_terminator()
    {
      if(cursor_.read_byte() != std::byte{0x00})
      {
        throw std::runtime_error("BSON string payload must be null-terminated");
      }
    }

    static void expect_document_terminator(std::byte last)
    {
      if(last != std::byte{0x00})
      {
        throw std::runtime_error("BSON document missing terminator");
      }
    }

    template <typename Buffer> void read_payload(Buffer& out, std::size_t n)
    {
      const auto at = out.size();
      if constexpr(cursor_t::sized_input)
      {
        out.resize(at + n);
        cursor_.read_bytes_into(reinterpret_cast<std::byte*>(out.data() + at), n);
      }
      else
      {
        constexpr std::size_t max_unchecked_reserve = 64 * 1024;
        out.reserve(at + std::min(n, max_unchecked_reserve));
        for(std::size_t i = 0; i < n; ++i)
        {
          out.push_back(std::bit_cast<typename Buffer::value_type>(cursor_.read_byte()));
        }
      }
    }

    std::pair<std::size_t, std::byte> binary_header(detail::bson_type type)
    {
      if(type != detail::bson_type::binary)
      {
        throw std::runtime_error("Expected BSON binary type");
      }
      const auto length = cursor_.template read<std::int32_t>();
      if(length < 0)
      {
        throw std::runtime_error("Invalid BSON binary length");
      }
      const auto n = static_cast<std::size_t>(length);
      cursor_.require(n);
      return {n, cursor_.read_byte()};
    }

    void read_binary(detail::bson_type type, bson::binary& value)
    {
      auto [n, subtype] = binary_header(type);
      if(subtype == detail::binary_old_subtype)
      {
        if(n < sizeof(std::int32_t))
        {
          throw std::runtime_error("BSON binary subtype 0x02 is too short for its inner length");
        }
        n -= sizeof(std::int32_t);
        const auto inner = cursor_.template read<std::int32_t>();
        if(inner < 0 or static_cast<std::size_t>(inner) != n)
        {
          throw std::runtime_error(
              std::format(
                  "BSON binary subtype 0x02 inner length {} does not match its {} payload bytes",
                  inner, n));
        }
      }
      value.subtype = subtype;
      value.data.clear();
      read_payload(value.data, n);
    }

    void read_code_w_scope(bson::code_w_scope& value)
    {
      const auto limit = code_w_scope_limit();

      value.code.clear();
      read_payload(value.code, string_length(limit) - 1);
      expect_string_terminator();

      const auto body = document_body_length(limit);
      value.scope.clear();
      detail::append(value.scope, static_cast<std::int32_t>(body + sizeof(std::int32_t)));
      read_payload(value.scope, body);
      expect_document_terminator(value.scope.back());

      end_code_w_scope(limit);
    }

    void skip_string(std::size_t limit)
    {
      cursor_.skip(string_length(limit) - 1);
      expect_string_terminator();
    }

    void skip_document(std::size_t limit)
    {
      cursor_.skip(document_body_length(limit) - 1);
      expect_document_terminator(cursor_.read_byte());
    }

    void skip_element(detail::bson_type type)
    {
      constexpr auto unbounded = std::numeric_limits<std::size_t>::max();
      switch(type)
      {
        case detail::bson_type::null:
        case detail::bson_type::undefined:
        case detail::bson_type::min_key:
        case detail::bson_type::max_key:
          return;
        case detail::bson_type::boolean:
          cursor_.skip(1);
          return;
        case detail::bson_type::int32:
          cursor_.skip(4);
          return;
        case detail::bson_type::double_:
        case detail::bson_type::int64:
        case detail::bson_type::datetime:
        case detail::bson_type::timestamp:
          cursor_.skip(8);
          return;
        case detail::bson_type::object_id:
          cursor_.skip(12);
          return;
        case detail::bson_type::decimal128:
          cursor_.skip(16);
          return;
        case detail::bson_type::string:
        case detail::bson_type::javascript:
        case detail::bson_type::symbol:
          skip_string(unbounded);
          return;
        case detail::bson_type::binary:
        {
          const auto size = cursor_.template read<std::int32_t>();
          if(size < 0)
          {
            throw std::runtime_error("Invalid BSON binary length");
          }
          const auto n = static_cast<std::size_t>(size) + 1;
          cursor_.require(n);
          cursor_.skip(n);
          return;
        }
        case detail::bson_type::document:
        case detail::bson_type::array:
          skip_document(unbounded);
          return;
        case detail::bson_type::code_w_scope:
        {
          const auto limit = code_w_scope_limit();
          skip_string(limit);
          skip_document(limit);
          end_code_w_scope(limit);
          return;
        }
        case detail::bson_type::db_pointer:
          skip_string(unbounded);
          cursor_.skip(12);
          return;
        case detail::bson_type::regex:
          cursor_.skip_cstring();
          cursor_.skip_cstring();
          return;
        default:
          throw std::runtime_error(std::format(
              "BSON: unknown element type 0x{:02x}", std::to_underlying(type)));
      }
    }

    template <typename Key, typename Value>
    void read_member_or_miss(detail::bson_type type, Key const& key, Value& value)
    {
      if(not object_try_visit_flat(
             key, value, [this, type](auto& member) { read_element(type, member); }))
      {
        if constexpr(Opts.unknown_keys == serde::unknown_keys::skip)
        {
          skip_element(type);
        }
        else
        {
          throw std::runtime_error(std::format("BSON: no member matches the key '{}'", key));
        }
      }
    }
  };

  /** @brief The one way into the BSON deserializer's private members, for the namespace-scope
   * readers.
   */
  struct detail::bson_access
  {
    template <typename De, typename Key, typename Value>
    static void read_member_or_miss(De& de, bson_type type, Key const& key, Value& value)
    {
      de.read_member_or_miss(type, key, value);
    }
  };

  template <std::input_iterator It> deserializer(It, It) -> deserializer<It>;
  template <std::ranges::input_range R>
  deserializer(R&&) -> deserializer<std::ranges::iterator_t<R>>;

  template <std::input_iterator It, auto X>
  deserializer(It, It, std::constant_wrapper<X>) -> deserializer<It, X>;
  template <std::ranges::input_range R, auto X>
  deserializer(R&&, std::constant_wrapper<X>) -> deserializer<std::ranges::iterator_t<R>, X>;
  template <typename CharT, typename CharTrait, auto X>
  deserializer(std::basic_istream<CharT, CharTrait>&, std::constant_wrapper<X>)
      -> deserializer<std::istreambuf_iterator<CharT>, X>;

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

  /** @brief Write @p value as the Extended JSON v2 document
   * `{"$binary": {"base64": ..., "subType": ...}}`, the subtype as two lowercase hex digits.
   *
   * Taken by the JSON, YAML and TOML serializers only. XML, CSV and BSON never see it.
   */
  template <serde::detail::ext_json_serializer_c S>
  auto tag_invoke(tag_t<serde::serialize>, S & ser, bson::binary const& value)
  {
    const auto subtype = std::array{value.subtype};
    return serde::serialize(
        ser, detail::ext_json_binary{
                 .binary = {
                            .base64   = serde::detail::base64_encode(value.data),
                            .sub_type = serde::detail::hex_encode(subtype),
                            }
    });
  }

  /** @brief Write @p value as its Extended JSON v2 document.
   *
   * `object_id` as `{"$oid": <24 lowercase hex>}`, `timestamp` as
   * `{"$timestamp": {"t": <seconds>, "i": <increment>}}`, `regex` as
   * `{"$regularExpression": {"pattern": ..., "options": ...}}`, `javascript` as `{"$code": ...}`,
   * `code_w_scope` as `{"$code": ..., "$scope": {...}}`, `symbol` as `{"$symbol": ...}`,
   * `db_pointer` as `{"$dbPointer": {"$ref": ..., "$id": {"$oid": ...}}}`, `min_key` as
   * `{"$minKey": 1}`, `max_key` as `{"$maxKey": 1}` and `undefined` as `{"$undefined": true}`.
   *
   * The scope of a `code_w_scope` is decoded as a BSON document and written with the same rules.
   * A malformed scope throws before any of this value's own text is written. Text already
   * written for enclosing values, such as earlier members and the key, stays in the output. Taken
   * by the JSON, YAML and TOML serializers only. XML, CSV and BSON never see it.
   */
  template <serde::detail::ext_json_serializer_c S, detail::bson_extended_c T>
  auto tag_invoke(tag_t<serde::serialize>, S & ser, T const& value)
  {
    if constexpr(std::same_as<T, bson::code_w_scope>)
    {
      detail::check_scope(value.scope);
      return serde::serialize(
          ser, detail::ext_json_code_w_scope{
                   .code  = value.code,
                   .scope = deserializer{value.scope}.template load<bson::object>(),
               });
    }
    else
    {
      return serde::serialize(ser, detail::ext_json_of(value));
    }
  }

  template <typename It, read_options Opts>
  auto tag_invoke(tag_default_t<serde::deserialize>, deserializer<It, Opts> & de)
  {
    return deserialize(de, std::type_identity<bson::value>{});
  }

  template <std::input_iterator It, read_options Opts, typename T>
  T tag_invoke(
      tag_default_t<serde::deserialize>, deserializer<It, Opts> & de, std::type_identity<T>)
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
          detail::bson_access::read_member_or_miss(de, type, key, value);
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

namespace reflex::serde::detail
{
template <> inline constexpr bool ext_json_type_v<bson::binary> = true;

template <bson::detail::bson_extended_c T> inline constexpr bool ext_json_type_v<T> = true;

template <> struct ext_json_reader<bson::value>
{
  static constexpr bool enabled = true;
};
}

REFLEX_EXPORT namespace reflex::serde::ser
{
  constexpr auto bson = ^^reflex::serde::bson::serializer;
}

REFLEX_EXPORT namespace reflex::serde::de
{
  constexpr auto bson = ^^reflex::serde::bson::deserializer;
}
