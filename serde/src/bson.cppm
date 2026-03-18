export module reflex.serde.bson;

export import reflex.serde;

export import :value;

import std;

namespace reflex::serde::bson::detail
{
using bytes = std::vector<std::byte>;

template <typename T>
concept bson_scalar_c = std::same_as<std::decay_t<T>, bson::decimal128>
                     or std::same_as<std::decay_t<T>, bson::datetime>;

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

constexpr void append(bytes& out, bson::decimal128 const& value)
{
  out.insert(out.end(), value.bytes.begin(), value.bytes.end());
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
    append(out, detail::bson_type::double_);
    append(out, key);
    append(out, static_cast<double>(value));
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
        value
        >= std::numeric_limits<std::int32_t>::min()
        and value
        <= std::numeric_limits<std::int32_t>::max())
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

} // namespace detail

export namespace reflex::serde::bson
{
class serializer
{
public:
  template <typename Out, typename T>
  constexpr Out& operator()(Out& out, T const& value) const
    requires requires(Out& o, std::byte b) { o.push_back(b); }
  {
    auto encoded = detail::encode_root(value);
    for(std::byte byte : encoded)
    {
      out.push_back(byte);
    }
    return out;
  }

  template <typename Out, typename T>
  constexpr Out& operator()(Out& out, T const& value) const
    requires requires(Out& o, std::byte b) {
      typename Out::char_type;
      sizeof(typename Out::char_type) == sizeof(std::byte);
      o << typename Out::char_type(b);
    }
  {
    using char_type = typename Out::char_type;
    auto encoded    = detail::encode_root(value);
    for(std::byte byte : encoded)
    {
      out << char_type(byte);
    }
    return out;
  }
};

class deserializer
{
private:
  template <std::input_iterator It, std::sentinel_for<It> End>
  using range_cursor = std::ranges::subrange<It, End>;

  template <typename R> struct cursor
  {
    R           range;
    std::size_t position = 0;

    constexpr bool at_end() const
    {
      return range.empty();
    }

    constexpr std::byte read()
    {
      if(at_end())
      {
        throw std::runtime_error("Unexpected end of BSON input");
      }
      using item_t = std::remove_cvref_t<std::ranges::range_reference_t<R>>;
      const auto byte = std::bit_cast<std::byte>(*range.begin());
      range.advance(1);
      ++position;
      return byte;
    }

    template <std::integral T> constexpr T read()
    {
      using unsigned_t = std::make_unsigned_t<T>;
      unsigned_t raw   = 0;
      for(std::size_t i = 0; i < sizeof(T); ++i)
      {
        raw |= static_cast<unsigned_t>(std::to_integer<unsigned int>(read())) << (8 * i);
      }
      return static_cast<T>(raw);
    }

    template <std::same_as<double> T> constexpr T read()
    {
      return std::bit_cast<T>(read<std::uint64_t>());
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
        out.reserve(static_cast<std::size_t>(size - 1));
        for(std::int32_t i = 0; i < size - 1; ++i)
        {
          out.push_back(static_cast<char>(std::to_integer<unsigned char>(read())));
        }
        if(read() != std::byte{0x00})
        {
          throw std::runtime_error("BSON string payload must be null-terminated");
        }
      }
      else
      {
        while(true)
        {
          const std::byte b = read();
          if(b == std::byte{0x00})
          {
            break;
          }
          out.push_back(static_cast<char>(std::to_integer<unsigned char>(b)));
        }
      }
      return out;
    }

    template <std::same_as<bson::decimal128> T> constexpr T read()
    {
      T out;
      for(std::size_t i = 0; i < out.bytes.size(); ++i)
      {
        out.bytes[i] = read();
      }
      return out;
    }
  };

  template <typename Cur> static constexpr std::size_t begin_document(Cur& cur)
  {
    const auto start = cur.position;
    const auto size  = cur.template read<std::int32_t>();
    if(size < 5)
    {
      throw std::runtime_error("Invalid BSON document length");
    }
    return start + static_cast<std::size_t>(size);
  }

  template <typename Cur, typename Fn> static constexpr void read_document(Cur& cur, Fn&& fn)
  {
    const std::size_t end_pos = begin_document(cur);
    while(cur.position < end_pos)
    {
      const auto type = static_cast<detail::bson_type>(cur.read());
      if(type == detail::bson_type::eof)
      {
        if(cur.position != end_pos)
        {
          throw std::runtime_error("Unexpected BSON bytes after terminator");
        }
        return;
      }

      auto key = cur.template read<std::string>();
      std::forward<Fn>(fn)(type, key);
      if(cur.position > end_pos)
      {
        throw std::runtime_error("BSON document element exceeds declared size");
      }
    }

    throw std::runtime_error("BSON document missing terminator");
  }

  template <typename Cur, typename T>
  static constexpr void read_payload(detail::bson_type type, Cur& cur, T& value)
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
      auto raw = cur.read();
      value    = (raw != std::byte{0x00});
    }
    else if constexpr(str_c<value_t>)
    {
      if(type != detail::bson_type::string)
      {
        throw std::runtime_error("Expected BSON string type");
      }
      value = cur.template read<std::string, true>();
    }
    else if constexpr(std::same_as<value_t, bson::int32>)
    {
      if(type != detail::bson_type::int32)
      {
        throw std::runtime_error("Expected BSON int32 type");
      }
      value = cur.template read<bson::int32>();
    }
    else if constexpr(std::same_as<value_t, bson::int64>)
    {
      if(type != detail::bson_type::int64)
      {
        throw std::runtime_error("Expected BSON int64 type");
      }
      value = cur.template read<bson::int64>();
    }
    else if constexpr(std::same_as<value_t, bson::decimal128>)
    {
      if(type != detail::bson_type::decimal128)
      {
        throw std::runtime_error("Expected BSON decimal128 type");
      }
      value = cur.template read<bson::decimal128>();
    }
    else if constexpr(std::same_as<value_t, bson::datetime>)
    {
      if(type != detail::bson_type::datetime)
      {
        throw std::runtime_error("Expected BSON datetime type");
      }
      value.millis_since_epoch = cur.template read<std::int64_t>();
    }
    else if constexpr(number_c<value_t>)
    {
      switch(type)
      {
        case detail::bson_type::int32:
          value = static_cast<value_t>(cur.template read<std::int32_t>());
          return;
        case detail::bson_type::int64:
          value = static_cast<value_t>(cur.template read<std::int64_t>());
          return;
        case detail::bson_type::double_:
          value = static_cast<value_t>(cur.template read<double>());
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
      read_document(cur, [&cur, &value](detail::bson_type elem_type, std::string const&) {
        typename value_t::value_type elem{};
        read_payload(elem_type, cur, elem);
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
      read_document(cur, [&cur, &value](detail::bson_type elem_type, std::string const& key) {
        typename value_t::mapped_type mapped{};
        read_payload(elem_type, cur, mapped);
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
      read_document(cur, [&cur, &value](detail::bson_type elem_type, std::string const& key) {
        object_visit(
            key, value, [&cur, elem_type](auto& member) { read_payload(elem_type, cur, member); });
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
          value = (cur.read() != std::byte{0x00});
          return;
        case detail::bson_type::int32:
          value = cur.template read<bson::int32>();
          return;
        case detail::bson_type::int64:
          value = cur.template read<bson::int64>();
          return;
        case detail::bson_type::double_:
          value = cur.template read<double>();
          return;
        case detail::bson_type::string:
          value = cur.template read<std::string, true>();
          return;
        case detail::bson_type::decimal128:
          value = cur.template read<bson::decimal128>();
          return;
        case detail::bson_type::datetime:
          value = bson::datetime{cur.template read<std::int64_t>()};
          return;
        case detail::bson_type::document:
        {
          auto& object = value.template emplace<bson::object>();
          object.clear();
          read_document(cur, [&cur, &object](detail::bson_type elem_type, std::string const& key) {
            bson::value nested;
            read_payload(elem_type, cur, nested);
            object[key] = std::move(nested);
          });
          return;
        }
        case detail::bson_type::array:
        {
          auto& array = value.template emplace<bson::array>();
          array.clear();
          read_document(cur, [&cur, &array](detail::bson_type elem_type, std::string const&) {
            bson::value nested;
            read_payload(elem_type, cur, nested);
            array.push_back(std::move(nested));
          });
          return;
        }
        default:
          throw std::runtime_error("Unsupported BSON type tag");
      }
    }
    else if constexpr(requires(value_t& v) { reflex::visit([](auto const&) {}, v); })
    {
      read_payload(type, cur, value.template emplace<bson::value>());
    }
    else
    {
      static_assert(false, "Unsupported BSON target type");
    }
  }

  template <typename Cur, typename T> static constexpr void read_root(Cur& cur, T& value)
  {
    if constexpr(std::same_as<std::decay_t<T>, bson::value>)
    {
      bson::object root;
      read_document(cur, [&cur, &root](detail::bson_type type, std::string const& key) {
        bson::value member_value;
        read_payload(type, cur, member_value);
        root[key] = std::move(member_value);
      });

      // Scalars are wrapped as {"value": <scalar>} at the BSON root.
      if(root.size() == 1)
      {
        auto it = root.find("value");
        if(it != root.end())
        {
          value = std::move(it->second);
          return;
        }
      }

      value = std::move(root);
      return;
    }

    if constexpr(
        map_c<T>
        or (object_visitable_c<T>
            and !std::same_as<std::decay_t<T>, bson::value>
            and !detail::bson_scalar_c<T>))
    {
      read_document(cur, [&cur, &value](detail::bson_type type, std::string const& key) {
        if constexpr(map_c<T>)
        {
          typename std::decay_t<T>::mapped_type mapped{};
          read_payload(type, cur, mapped);
          value[typename std::decay_t<T>::key_type{key}] = std::move(mapped);
        }
        else
        {
          object_visit(key, value, [&cur, type](auto& member) { read_payload(type, cur, member); });
        }
      });
      return;
    }

    bool has_value = false;
    read_document(cur, [&cur, &value, &has_value](detail::bson_type type, std::string const& key) {
      if(key != "value")
      {
        throw std::runtime_error("Expected wrapped BSON scalar key 'value'");
      }
      if(has_value)
      {
        throw std::runtime_error("Duplicate wrapped BSON scalar key");
      }
      has_value = true;
      read_payload(type, cur, value);
    });

    if(!has_value)
    {
      throw std::runtime_error("Missing wrapped BSON scalar key");
    }
  }

public:
  deserializer() = default;

  template <typename T = bson::value, std::ranges::input_range R> static T load(R&& input)
  {
    auto sub = std::ranges::subrange(std::ranges::begin(input), std::ranges::end(input));
    cursor<decltype(sub)> cur{std::move(sub)};
    T                     value{};
    read_root(cur, value);
    if(!cur.at_end())
    {
      throw std::runtime_error("Unexpected trailing BSON input");
    }
    return value;
  }

  template <typename T = bson::value, std::input_iterator It, std::sentinel_for<It> End>
  static T load(It first, End last)
  {
    return load<T>(range_cursor<It, End>{first, last});
  }

  template <typename T = bson::value> static T load(std::span<const std::byte> in)
  {
    return load<T>(in.begin(), in.end());
  }

  template <typename T = bson::value, std::ranges::contiguous_range Buffer>
  static T load(Buffer const& in)
    requires std::ranges::sized_range<Buffer>
  {
    using element_t = std::remove_cv_t<std::ranges::range_value_t<Buffer>>;
    auto bytes =
        std::as_bytes(std::span<element_t const>(std::ranges::data(in), std::ranges::size(in)));
    return load<T>(bytes.begin(), bytes.end());
  }

  template <typename T = bson::value, std::input_iterator It, std::sentinel_for<It> End>
  static T operator()(It first, End last)
  {
    return load<T>(first, last);
  }

  template <typename T = bson::value> static T operator()(auto&& in)
  {
    return load<T>(std::forward<decltype(in)>(in));
  }
};

} // namespace reflex::serde::bson

export namespace reflex::serde::ser
{
using bson = reflex::serde::bson::serializer;
}

export namespace reflex::serde::de
{
using bson = reflex::serde::bson::deserializer;
}
