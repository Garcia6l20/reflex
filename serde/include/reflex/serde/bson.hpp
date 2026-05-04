#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <reflex/serde/bson_value.hpp>
#endif

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
  for(std::size_t i = 0; i < sizeof(T); ++i)
  {
    out.push_back(static_cast<std::byte>((raw >> (8 * i)) & 0xFFu));
  }
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
  for(char ch : value)
  {
    out.push_back(static_cast<std::byte>(static_cast<unsigned char>(ch)));
  }
  out.push_back(std::byte{0x00});
}

template <typename T>
constexpr void write_element(bytes& out, std::string_view key, T const& value);

template <typename Fn> constexpr bytes make_document(Fn&& write_elements)
{
  bytes out{};

  // Reserve space for 4-byte BSON document size.
  append(out, std::int32_t{0});
  std::forward<Fn>(write_elements)(out);
  append(out, std::byte{0x00});

  auto size = static_cast<std::int32_t>(out.size());
  for(std::size_t i = 0; i < sizeof(std::int32_t); ++i)
  {
    out[i] = static_cast<std::byte>((size >> (8 * i)) & 0xFF);
  }

  return out;
}

template <typename T>
constexpr void write_document_value(bytes& out, std::string_view key, T const& value)
{
  append(out, detail::bson_type::document);
  append(out, key);

  auto nested = make_document([&](bytes& doc) {
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
        auto const&                member_val  = value.[:member:];
        reflex::visit([&](auto const& v) { write_element(doc, member_name, v); }, member_val);
      }
    }
  });

  out.insert(out.end(), nested.begin(), nested.end());
}

template <typename T>
constexpr void write_array_value(bytes& out, std::string_view key, T const& value)
{
  append(out, detail::bson_type::array);
  append(out, key);

  auto nested = make_document([&](bytes& doc) {
    std::size_t idx = 0;
    for(auto const& element : value)
    {
      auto index = std::to_string(idx++);
      reflex::visit([&](auto const& v) { write_element(doc, index, v); }, element);
    }
  });

  out.insert(out.end(), nested.begin(), nested.end());
}

template <typename T> constexpr void write_element(bytes& out, std::string_view key, T const& value)
{
  using value_t = std::decay_t<T>;

  if constexpr(std::same_as<value_t, null_t>)
  {
    append(out, detail::bson_type::null);
    append(out, key);
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
  else if constexpr(requires() { reflex::visit([](auto const&) {}, value); })
  {
    reflex::visit([&](auto const& inner) { write_element(out, key, inner); }, value);
  }
  else
  {
    static_assert(false, "Unsupported BSON value type");
  }
}

template <typename T> constexpr bytes encode_root(T const& value)
{
  using U = std::decay_t<T>;
  if constexpr(map_c<U> or (aggregate_c<U> and !bson_scalar_c<U>))
  {
    return make_document([&](bytes& out) {
      if constexpr(map_c<U>)
      {
        for(auto const& [member_key, member_value] : value)
        {
          write_element(out, std::string_view(member_key), member_value);
        }
      }
      else
      {
        template for(constexpr auto member : define_static_array(
                         nonstatic_data_members_of(^^U, std::meta::access_context::current())))
        {
          constexpr std::string_view member_name = serialized_name(member);
          auto const&                member_val  = value.[:member:];
          reflex::visit([&](auto const& v) { write_element(out, member_name, v); }, member_val);
        }
      }
    });
  }
  else if constexpr(reflex::visitable_c<U>)
  {
    return reflex::visit([&](auto const& v) { return encode_root(v); }, value);
  }
  else
  {
    return make_document([&](bytes& out) { write_element(out, "value", value); });
  }
}

} // namespace reflex::serde::bson::detail

REFLEX_EXPORT namespace reflex::serde::bson
{
  template <typename OutputIt>
  concept bson_output_iterator_c = std::output_iterator<OutputIt, std::byte>
                                or std::output_iterator<OutputIt, char>
                                or std::output_iterator<OutputIt, unsigned char>;

  template <bson_output_iterator_c OutputIt> class serializer
  {
    OutputIt out_;

  public:
    explicit serializer(OutputIt out) : out_(out)
    {}

    template <typename T>
      requires std::constructible_from<OutputIt, T&>
    serializer(T& out) : out_(OutputIt(out))
    {}

    constexpr OutputIt& out()
    {
      return out_;
    }

    template <typename T> constexpr void dump(T const& value)
    {
      if constexpr(requires { serialize(*this, value); })
      {
        serialize(*this, value);
      }
      else
      {
        static_assert(false, std::string(display_string_of(^^T)) + " is not serializable to BSON");
      }
    }
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
      range_type  range;
      std::size_t position = 0;

      constexpr bool at_end() const
      {
        return range.empty();
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
        // if(n > std::ranges::distance(range))
        // {
        //   throw std::runtime_error("Unexpected end of BSON input");
        // }
        range.advance(n);
        position += n;
      }

      template <std::size_t N> constexpr auto read_bytes()
      {
        std::array<std::byte, N> bytes{};
        for(std::size_t i = 0; i < N; ++i)
        {
          bytes[i] = read_byte();
        }
        return bytes;
      }

      template <typename T> constexpr auto read_as()
      {
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

        constexpr bool
            contiguous_byte_or_char = std::contiguous_iterator<It>
                                  and (decays_to_c<std::iter_value_t<It>, std::byte>
                                       or decays_to_c<std::iter_value_t<It>, char>
                                       or decays_to_c<std::iter_value_t<It>, unsigned char>);

        if constexpr(has_size)
        {
          auto size = read<std::int32_t>();
          if(size <= 0)
          {
            throw std::runtime_error("Invalid BSON string length");
          }

          if constexpr(contiguous_byte_or_char)
          {
            const auto* first = std::to_address(range.begin());
            out.assign(reinterpret_cast<char const*>(first), static_cast<std::size_t>(size - 1));
            advance(static_cast<std::size_t>(size - 1));
          }
          else
          {
            out.reserve(static_cast<std::size_t>(size - 1));
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
          if constexpr(contiguous_byte_or_char)
          {
            const auto* first = std::to_address(range.begin());
            const auto* ptr   = first;
            const auto* last  = std::to_address(range.end());

            while(ptr != last and std::bit_cast<std::byte>(*ptr) != std::byte{0x00})
            {
              ++ptr;
            }

            if(ptr == last)
            {
              throw std::runtime_error("BSON cstring payload must be null-terminated");
            }

            out.assign(reinterpret_cast<char const*>(first), static_cast<std::size_t>(ptr - first));
            advance(static_cast<std::size_t>(ptr - first) + 1);
          }
          else
          {
            while(true)
            {
              const std::byte b = read_byte();
              if(b == std::byte{0x00})
              {
                break;
              }
              out.push_back(static_cast<char>(std::to_integer<unsigned char>(b)));
            }
          }
        }
        return out;
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

        auto key = cursor_.template read<std::string>();
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
        auto decoded = cursor_.template read<std::string, true>();
        if constexpr(requires { value = decoded; })
        {
          value = decoded;
        }
        else
        {
          static_assert(
              false, "BSON string deserialization requires an assignable owning string target");
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
          object_visit(
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
  OutputIt tag_invoke(tag_t<serde::serialize>, serializer<OutputIt> & ser, T const& value)
  {
    auto& out     = ser.out();
    auto  encoded = detail::encode_root(value);

    if constexpr(std::output_iterator<OutputIt, std::byte>)
    {
      std::ranges::copy(encoded, out);
    }
    else if constexpr(std::output_iterator<OutputIt, char>)
    {
      for(std::byte b : encoded)
      {
        *out++ = static_cast<char>(std::to_integer<unsigned char>(b));
      }
    }
    else if constexpr(std::output_iterator<OutputIt, unsigned char>)
    {
      for(std::byte b : encoded)
      {
        *out++ = static_cast<unsigned char>(std::to_integer<unsigned char>(b));
      }
    }

    return out;
  }

  template <typename It> auto tag_invoke(tag_t<serde::deserialize>, deserializer<It> & de)
  {
    return deserialize(de, std::type_identity<bson::value>{});
  }

  template <std::input_iterator It, typename T>
  T tag_invoke(tag_t<serde::deserialize>, deserializer<It> & de, std::type_identity<T>)
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
          object_visit(key, value, [&](auto& member) { de.read_element(type, member); });
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
