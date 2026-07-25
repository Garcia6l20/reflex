#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <cstring>

#include <reflex/format.hpp>
#include <reflex/heapless/string.hpp>
#include <reflex/parse.hpp>
#include <reflex/serde.hpp>
#include <reflex/serde/json_value.hpp>
#endif

#include <reflex/serde/detail/io.hpp>

REFLEX_EXPORT namespace reflex::serde::json
{
  namespace detail
  {
  template <typename var_type>
    requires(meta::is_template_instance_of(^^var_type, ^^poly::var))
  constexpr auto aggregate_types_of_var()
  {
    static constexpr auto types = []() {
      std::vector<std::meta::info> types;
      template for(constexpr auto t : var_type::infos::base_types)
      {
        using T = [:t:];
        if constexpr(aggregate_c<T> and dealias(t) != dealias(^^null_t))
        {
          types.push_back(t);
        }
      }
      return define_static_array(types);
    }();
    return types;
  }
  template <typename var_type>
    requires(not meta::is_template_instance_of(^^var_type, ^^poly::var))
  constexpr auto aggregate_types_of_var()
  {
    static constexpr auto empty = std::array<std::meta::info, 0>{};
    return empty;
  }

  // One entry per byte value, true when the byte cannot appear literally inside a
  // JSON string: 0x00-0x1F, '"' and '\\'. '/' is deliberately absent, JSON allows
  // it to be escaped but does not require it. Bytes 0x80-0xFF are absent too, they
  // are UTF-8 lead and continuation bytes and pass through unchanged.
  inline constexpr std::array<bool, 256> escape_table = [] {
    std::array<bool, 256> t{};
    for(unsigned c = 0; c < 0x20; ++c)
    {
      t[c] = true;
    }
    t[static_cast<unsigned char>('"')]  = true;
    t[static_cast<unsigned char>('\\')] = true;
    return t;
  }();

  constexpr bool needs_escape(char c)
  {
    return escape_table[static_cast<unsigned char>(c)];
  }

  // A bulk scan is wrong here. JSON's escape set is thirty-four bytes, so a min
  // over find(char) is thirty-four memchr passes and find_first_of is a per-byte
  // loop rescanning all thirty-four. The table walk beats both.
  inline std::size_t find_escapable(std::string_view s)
  {
    const auto* const first = s.data();
    const auto* const last  = first + s.size();
    const auto*       it    = std::find_if(first, last, needs_escape);
    return it == last ? std::string_view::npos : static_cast<std::size_t>(it - first);
  }

  // Emits the two-character escape for the seven bytes JSON names, and \u00XX for
  // every other control character.
  template <typename Ser> void write_escape(Ser& ser, char c)
  {
    switch(c)
    {
      case '"':
        ser.write_raw("\\\"");
        return;
      case '\\':
        ser.write_raw("\\\\");
        return;
      case '\b':
        ser.write_raw("\\b");
        return;
      case '\f':
        ser.write_raw("\\f");
        return;
      case '\n':
        ser.write_raw("\\n");
        return;
      case '\r':
        ser.write_raw("\\r");
        return;
      case '\t':
        ser.write_raw("\\t");
        return;
      default:
      {
        static constexpr std::string_view hex = "0123456789abcdef";
        const auto                        u   = static_cast<unsigned char>(c);
        const char buf[6] = {'\\', 'u', '0', '0', hex[u >> 4], hex[u & 0x0F]};
        ser.write_raw(std::string_view{buf, sizeof(buf)});
        return;
      }
    }
  }

  // Writes the body of a JSON string: runs of clean bytes go out whole, only the
  // bytes needing an escape are handled one at a time.
  template <typename Ser> void write_escaped(Ser& ser, std::string_view text)
  {
    std::size_t pos = 0;
    while(pos < text.size())
    {
      const std::size_t n = find_escapable(text.substr(pos));
      if(n == std::string_view::npos)
      {
        ser.write_raw(text.substr(pos));
        return;
      }
      ser.write_raw(text.substr(pos, n));
      write_escape(ser, text[pos + n]);
      pos += n + 1;
    }
  }

  template <typename Ser> void write_quoted(Ser& ser, std::string_view text)
  {
    ser.write_char('"');
    write_escaped(ser, text);
    ser.write_char('"');
  }

  // `"name":` is a constant, so it is built once at compile time and promoted to
  // static storage.
  //
  // The name is checked here rather than escaped. Identifiers cannot need an
  // escape, but a serde::rename can carry an arbitrary string, and a key that
  // silently emitted an unescaped quote would produce invalid JSON.
  template <std::meta::info Member> consteval std::string_view quoted_key()
  {
    constexpr std::string_view name = serialized_name(Member);
    static_assert(
        std::ranges::none_of(name, needs_escape),
        "a JSON object key cannot contain a quote, a backslash or a control "
        "character: check the serde::rename annotation on this member");
    std::string s;
    s.reserve(name.size() + 3);
    s += '"';
    s += name;
    s += "\":";
    return {std::define_static_string(s), s.size()};
  }
  } // namespace detail

  template <typename OutputIt> class serializer : public serde::detail::serializer_base<OutputIt>
  {
  public:
    using serde::detail::serializer_base<OutputIt>::serializer_base;

    static constexpr std::string_view format_name = "JSON";

    friend OutputIt tag_invoke(tag_t<serde::serialize>, serializer<OutputIt>& ser, null_t const&)
    {
      ser.write_raw("null");
      return ser.out();
    }

    template <typename T>
    friend OutputIt tag_invoke(
        tag_t<serde::serialize>,
        serializer<OutputIt>&   ser,
        std::optional<T> const& value)
    {
      if(value.has_value())
      {
        serialize(ser, *value);
      }
      else
      {
        ser.write_raw("null");
      }
      return ser.out();
    }

    template <str_c Str>
    friend OutputIt tag_invoke(tag_t<serde::serialize>, serializer<OutputIt>& ser, Str const& value)
    {
      if constexpr(meta::is_template_instance_of(^^Str, ^^std::array))
      {
        detail::write_quoted(
            ser, std::string_view{value.data(), ::strnlen(value.data(), value.size())});
      }
      else
      {
        detail::write_quoted(ser, std::string_view{value});
      }
      return ser.out();
    }

    template <number_c Num>
    friend OutputIt tag_invoke(tag_t<serde::serialize>, serializer<OutputIt>& ser, Num const& value)
    {
      serde::detail::write_digits(ser, value);
      return ser.out();
    }

    template <std::same_as<char> Char>
    friend OutputIt tag_invoke(tag_t<serde::serialize>, serializer<OutputIt>& ser, Char value)
    {
      ser.write_char('"');
      if(detail::needs_escape(value))
      {
        detail::write_escape(ser, value);
      }
      else
      {
        ser.write_char(value);
      }
      ser.write_char('"');
      return ser.out();
    }

    template <std::same_as<boolean> Boolean>
    friend OutputIt tag_invoke(tag_t<serde::serialize>, serializer<OutputIt>& ser, Boolean value)
    {
      ser.write_raw(value ? "true" : "false");
      return ser.out();
    }

    template <derives_c<derive_t<Format>> T>
    friend OutputIt tag_invoke(tag_t<serde::serialize>, serializer<OutputIt>& ser, T const& value)
    {
      // The rendered text has to be scanned before it reaches the sink, so it
      // cannot be formatted straight through. Anything a formatter is likely to
      // produce here fits the stack buffer.
      std::array<char, 128> buf{};
      const auto            r = std::format_to_n(buf.data(), buf.size(), "{}", value);
      if(static_cast<std::size_t>(r.size) <= buf.size())
      {
        detail::write_quoted(ser, std::string_view{buf.data(), static_cast<std::size_t>(r.size)});
      }
      else
      {
        detail::write_quoted(ser, std::format("{}", value));
      }
      return ser.out();
    }

    template <seq_c Seq>
    friend OutputIt tag_invoke(tag_t<serde::serialize>, serializer<OutputIt>& ser, Seq const& value)
    {
      ser.write_char('[');
      if(value.empty())
      {
        ser.write_char(']');
        return ser.out();
      }

      auto view = std::views::all(value);

      for(const auto& elem : view | std::views::take(value.size() - 1))
      {
        serialize(ser, elem);
        ser.write_char(',');
      }
      serialize(ser, view.back());

      ser.write_char(']');
      return ser.out();
    }

    template <pair_c Pair>
    friend OutputIt
        tag_invoke(tag_t<serde::serialize>, serializer<OutputIt>& ser, Pair const& value)
    {
      ser.write_char('{');
      serialize(ser, value.first);
      ser.write_char(':');
      reflex::visit([&ser](const auto& v) { serialize(ser, v); }, value.second);
      ser.write_char('}');
      return ser.out();
    }

    template <map_c Map>
    friend OutputIt tag_invoke(tag_t<serde::serialize>, serializer<OutputIt>& ser, Map const& value)
    {
      ser.write_char('{');

      bool first = true;

      for(const auto& [key, val] : value)
      {
        if(not first)
        {
          ser.write_char(',');
        }
        else
        {
          first = false;
        }
        serialize(ser, key);
        ser.write_char(':');
        reflex::visit(
            [&ser]<typename U>(const U& v) {
              using var_type = typename Map::mapped_type;
              if constexpr(std::ranges::contains(detail::aggregate_types_of_var<var_type>(), ^^U))
              {
                serialize(ser, v, std::cw<true>);
              }
              else
              {
                serialize(ser, v);
              }
            },
            val);
      }
      ser.write_char('}');
      return ser.out();
    }

    template <visitable_c T>
    friend OutputIt tag_invoke(tag_t<serde::serialize>, serializer<OutputIt>& ser, T const& value)
    {
      return visit([&ser](const auto& v) { return serialize(ser, v); }, value);
    }
  };

  template <typename... TArgs>
  serializer(std::basic_string<TArgs...> & out)
      -> serializer<std::back_insert_iterator<std::basic_string<TArgs...>>>;
  serializer(std::ofstream & out) -> serializer<std::ostreambuf_iterator<char>>;
  serializer(std::ostringstream & out) -> serializer<std::ostreambuf_iterator<char>>;

  template <typename OutputIt, aggregate_c Agg, typename TagT = std::false_type>
    requires(not(str_c<Agg> or seq_c<Agg>)) // std::array<char> may be considered as an aggregate
  OutputIt tag_invoke(
      tag_default_t<serde::serialize>, serializer<OutputIt> & ser, Agg const& value, TagT = {})
  {
    ser.write_char('{');

    constexpr bool tag = TagT::value;

    bool first = not tag;

    if constexpr(tag)
    {
      ser.write_raw("\"__type\":");
      // Hashing at compile time would change the emitted id, which is a wire format
      // break.
      static auto expected_id = std::hash<std::string_view>{}(identifier_of(dealias(decay(^^Agg))));
      serialize(ser, expected_id);
    }

    static constexpr auto type = decay(type_of(^^value));
    template for(constexpr auto member : define_static_array(
                     nonstatic_data_members_of(type, std::meta::access_context::current())))
    {
      if(not first)
      {
        ser.write_char(',');
      }
      else
      {
        first = false;
      }
      static constexpr std::string_view key_token = detail::quoted_key<member>();
      ser.write_raw(key_token);
      auto const& member_value = value.[:member:];
      reflex::visit([&ser](const auto& v) { serialize(ser, v); }, member_value);
    }
    ser.write_char('}');
    return ser.out();
  }

  template <std::input_iterator InputIt>
  class deserializer : public serde::detail::subrange_deserializer<InputIt>
  {
    using base = serde::detail::subrange_deserializer<InputIt>;
    using base::cursor_;

  public:
    using base::at_end;
    using base::base;

    InputIt begin() const
    {
      return cursor_.begin();
    }

    InputIt end() const
    {
      return cursor_.end();
    }

    auto peek() const
    {
      if(at_end())
      {
        throw std::runtime_error("Unexpected end of JSON input");
      }
      return *cursor_.begin();
    }

    auto advance()
    {
      const auto c = peek();
      cursor_.advance(1);
      return c;
    }

    void advance_to(InputIt pos)
    {
      cursor_ = {pos, cursor_.end()};
    }

    void expect(std::string_view token)
    {
      for(char expected : token)
      {
        if(advance() != expected)
        {
          throw std::runtime_error(std::format("Expected '{}'", token));
        }
      }
    }

    void ltrim()
    {
      while(!at_end())
      {
        const auto ch = peek();
        if(reflex::is_space(ch))
        {
          advance();
        }
        else
        {
          break;
        }
      }
    }

    // makes load() without an explicit type read a json::value
    using default_load_type = json::value;

    friend auto tag_invoke(tag_t<serde::deserialize>, deserializer<InputIt>& de)
    {
      return deserialize(de, std::type_identity<json::value>{});
    }

    friend auto tag_invoke(
        tag_t<serde::deserialize>,
        deserializer<InputIt>& de,
        std::type_identity<json::null_t>)
    {
      de.expect("null");
      return null;
    }

    template <typename T>
    friend std::optional<T> tag_invoke(
        tag_t<serde::deserialize>,
        deserializer<InputIt>& de,
        std::type_identity<std::optional<T>>)
    {
      if(de.peek() == 'n')
      {
        de.expect("null");
        return std::nullopt;
      }
      else
      {
        return de.template load<T>();
      }
    }

    template <str_c Str>
    friend auto
        tag_invoke(tag_t<serde::deserialize>, deserializer<InputIt>& de, std::type_identity<Str>)
    {
      if(de.advance() != '"')
      {
        throw std::runtime_error("Expected '\"' at start of JSON string");
      }

      Str value{};

      auto push = [&value] {
        if constexpr(requires { value.push_back(char{}); })
        {
          return [&value](char c) { value.push_back(c); };
        }
        else
        {
          return [it = std::begin(value), end = std::end(value)](char c) mutable {
            if(it == end)
            {
              throw std::out_of_range("String too long to fit in target type");
            }
            *it++ = c;
          };
        }
      }();

      while(not de.at_end())
      {
        const char c = de.advance();
        if(c == '"')
        {
          return value;
        }

        if(c == '\\')
        {
          const char esc = de.advance();
          switch(esc)
          {
            case '"':
              push('"');
              break;
            case '\\':
              push('\\');
              break;
            case '/':
              push('/');
              break;
            case 'b':
              push('\b');
              break;
            case 'f':
              push('\f');
              break;
            case 'n':
              push('\n');
              break;
            case 'r':
              push('\r');
              break;
            case 't':
              push('\t');
              break;
            case 'u':
            {
              // Only the subset below 0x80 is decoded, which is exactly what the
              // serializer emits: a control character, as one byte of UTF-8.
              // Surrogate pairs and higher code points would need a multi-byte
              // encoding and still throw.
              int cp = 0;
              for(int i = 0; i < 4; ++i)
              {
                const char d = de.advance();
                int        n = -1;
                if(d >= '0' and d <= '9')
                {
                  n = d - '0';
                }
                else if(d >= 'a' and d <= 'f')
                {
                  n = d - 'a' + 10;
                }
                else if(d >= 'A' and d <= 'F')
                {
                  n = d - 'A' + 10;
                }
                else
                {
                  throw std::runtime_error(std::format("Invalid \\u escape digit: {}", d));
                }
                cp = (cp << 4) | n;
              }
              if(cp > 0x7F)
              {
                throw std::runtime_error("\\uXXXX escapes not implemented");
              }
              push(static_cast<char>(cp));
              break;
            }
            default:
              throw std::runtime_error(std::format("Unknown escape: \\{}", esc));
          }
        }
        else
        {
          push(c);
        }
      }
      throw std::runtime_error("Unterminated JSON string");
    }

    template <number_c Num>
    friend auto
        tag_invoke(tag_t<serde::deserialize>, deserializer<InputIt>& de, std::type_identity<Num>)
    {
      Num value;
      if constexpr(deserializer<InputIt>::bulk_scan)
      {
        const auto first = std::to_address(de.begin());
        const auto last  = std::to_address(de.end());
        auto [ptr, ec]   = std::from_chars(first, last, value);
        if(ec != std::errc{})
        {
          throw std::runtime_error("Failed to parse number");
        }
        const auto offset = ptr - first;
        de.advance_to(std::next(de.begin(), offset));
      }
      else
      {
        heapless::string<64> token{};
        while(not de.at_end())
        {
          const auto c = de.peek();
          if((c == '-') or (c == '+') or (c == '.') or (c == 'e') or (c == 'E') or is_digit(c))
          {
            token.push_back(c);
            de.advance();
          }
          else
          {
            break;
          }
        }
        auto [result, ec] = std::from_chars(token.data(), token.data() + token.size(), value);
        if(ec != std::errc{})
        {
          throw std::runtime_error("Failed to parse number");
        }
      }
      return value;
    }

    friend auto tag_invoke(
        tag_t<serde::deserialize>,
        deserializer<InputIt>& de,
        std::type_identity<boolean>)
    {
      if(de.peek() == 't')
      {
        de.expect("true");
        return true;
      }
      if(de.peek() == 'f')
      {
        de.expect("false");
        return false;
      }
      throw std::runtime_error("Expected 'true' or 'false'");
    }

    template <derives_c<derive_t<Parse>> T>
    friend auto
        tag_invoke(tag_t<serde::deserialize>, deserializer<InputIt>& de, std::type_identity<T>)
    {
      auto token  = de.template load<heapless::string<64>>();
      auto result = parse<std::remove_cvref_t<T>>(token);
      if(!result)
      {
        throw std::runtime_error(
            std::format(
                "Failed to parse value: {}", std::generic_category().message(int(result.error()))));
      }
      return std::move(result).value();
    }

    template <seq_c Seq>
    friend auto
        tag_invoke(tag_t<serde::deserialize>, deserializer<InputIt>& de, std::type_identity<Seq>)
    {
      if(de.advance() != '[')
        throw std::runtime_error("Expected '[' at start of JSON array");

      Seq value{};

      de.ltrim();
      if(not de.at_end() and de.peek() == ']')
      {
        de.advance();
        return value;
      }

      using elem_type = typename std::remove_cvref_t<decltype(value)>::value_type;

      auto push = [&value] {
        if constexpr(requires { value.push_back(elem_type{}); })
        {
          return [&value](elem_type&& elem) { value.push_back(std::forward<elem_type>(elem)); };
        }
        else
        {
          return [it = std::begin(value), end = std::end(value)](elem_type&& elem) mutable {
            if(it == end)
            {
              throw std::out_of_range("Array has more elements than target type can hold");
            }
            *it++ = std::forward<elem_type>(elem);
          };
        }
      }();

      while(true)
      {
        push(de.template load<elem_type>());

        de.ltrim();
        const char sep = de.advance();
        if(sep == ',')
        {
          de.ltrim();
          continue;
        }
        if(sep == ']')
        {
          break;
        }
        throw std::runtime_error("Expected ',' or ']' in array");
      }
      return value;
    }

    template <typename var_type>
      requires(meta::is_template_instance_of(^^var_type, ^^poly::var))
    friend auto tag_invoke(
        tag_t<serde::deserialize>,
        deserializer<InputIt>& de,
        std::type_identity<var_type>) -> var_type
    {
      de.ltrim();
      switch(de.peek())
      {
        case 't':
        case 'f':
          return de.template load<boolean>();
        case 'n':
          return de.template load<null_t>();
        case '{':
          return de.template load<typename var_type::obj_type>();
        case '[':
          return de.template load<typename var_type::arr_type>();
        case '"':
          return de.template load<json::string>();
        default:
          return de.template load<json::number>();
      }
    }
  };

  REFLEX_SERDE_DESERIALIZER_DEDUCTION_GUIDES(deserializer);

  template <typename InputIt, object_visitable_c Map>
    requires(
        not(meta::is_template_instance_of(^^Map, ^^poly::var)
            // std::array<char> may be considered as a visitable object
            or str_c<Map>
            or seq_c<Map>))
  auto tag_invoke(
      tag_default_t<serde::deserialize>, deserializer<InputIt> & de, std::type_identity<Map>)
  {
    if(de.advance() != '{')
      throw std::runtime_error("Expected '{' at start of JSON object");

    de.ltrim();
    if(!de.at_end() and de.peek() == '}')
    {
      de.advance();
      return Map{};
    }

    Map value{};
    while(true)
    {
      de.ltrim();
      auto key = de.template load<std::string>();
      de.ltrim();
      if(de.advance() != ':')
      {
        throw std::runtime_error("Expected ':' after object key");
      }
      de.ltrim();

      serde::object_visit(std::move(key), value, [&]<typename V>(V& v) {
        v = de.template load<std::remove_cvref_t<V>>();
        if constexpr(meta::is_template_instance_of(^^V, ^^poly::var))
        {
          if(v.is_object())
          {
            auto& obj = v.as_object();
            auto  type_key =
                std::ranges::find_if(obj, [](auto const& pair) { return pair.first == "__type"; });
            if(type_key != obj.end())
            {
              template for(constexpr auto a : detail::aggregate_types_of_var<V>())
              {
                if(type_key->second
                   == std::hash<std::string_view>{}(identifier_of(dealias(decay(a)))))
                {
                  using Agg = [:a:];
                  Agg agg_value{};
                  template for(constexpr auto member :
                               define_static_array(nonstatic_data_members_of(
                                   a, std::meta::access_context::current())))
                  {
                    constexpr std::string_view name = serialized_name(member);
                    if(name == "__type")
                    {
                      continue;
                    }
                    [[maybe_unused]] auto object_value_it = std::ranges::find_if(
                        obj, [name](auto const& pair) { return pair.first == name; });
                    auto& member_value = agg_value.[:member:];
                    visit(
                        [&]<typename U>([[maybe_unused]] U&& v) {
                          if constexpr(requires { member_value = std::forward<U>(v); })
                          {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wfloat-conversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
                            member_value = std::forward<U>(v);
#pragma GCC diagnostic pop
                          }
                          else
                          {
                            throw std::runtime_error("Cannot assign value to member");
                          }
                        },
                        std::move(object_value_it->second));
                  }
                  v = std::move(agg_value);
                  break;
                }
              }
            }
          }
        }
      });

      de.ltrim();
      const char sep = de.advance();
      if(sep == ',')
      {
        continue;
      }
      if(sep == '}')
      {
        break;
      }
      throw std::runtime_error("Expected ',' or '}' in object");
    }
    return value;
  }

} // namespace reflex::serde::json

REFLEX_EXPORT namespace reflex::serde::ser
{
  constexpr auto json = ^^reflex::serde::json::serializer;
}

REFLEX_EXPORT namespace reflex::serde::de
{
  constexpr auto json = ^^reflex::serde::json::deserializer;
}
