#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <reflex/parse.hpp>
#include <reflex/serde.hpp>
#include <reflex/serde/json_value.hpp>
#endif

REFLEX_EXPORT namespace reflex::serde::json
{
  template <typename T>
  concept serializable_c = str_c<std::remove_cvref_t<T>>
                        or number_c<std::remove_cvref_t<T>>
                        or std::same_as<std::remove_cvref_t<T>, boolean>
                        or std::same_as<std::remove_cvref_t<T>, null_t>
                        or seq_c<std::remove_cvref_t<T>>
                        or pair_c<std::remove_cvref_t<T>>
                        or map_c<std::remove_cvref_t<T>>
                        or aggregate_c<std::remove_cvref_t<T>>
                        or visitable_c<std::remove_cvref_t<T>>;

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
  } // namespace detail

  template <typename OutputIt> class serializer
  {
    OutputIt out_;

  public:
    serializer(OutputIt out) : out_(out)
    {}

    template <typename T>
      requires std::constructible_from<OutputIt, T&>
    serializer(T& out) : out_(OutputIt(out))
    {}

    constexpr OutputIt& out()
    {
      return out_;
    }

    template <typename T, bool tag = false> constexpr void dump(T const& value)
    {
      if constexpr(requires { serialize(*this, value); })
      {
        serialize(*this, value);
      }
      else
      {
        static_assert(false, std::string(display_string_of(^^T)) + " is not serializable to JSON");
      }
    }
  };

  template <typename... TArgs>
  serializer(std::basic_string<TArgs...> & out)
      -> serializer<std::back_insert_iterator<std::basic_string<TArgs...>>>;
  serializer(std::ofstream & out) -> serializer<std::ostreambuf_iterator<char>>;
  serializer(std::ostringstream & out) -> serializer<std::ostreambuf_iterator<char>>;

  template <typename OutputIt>
  OutputIt tag_invoke(tag_t<serde::serialize>, serializer<OutputIt> & ser, null_t const&)
  {
    std::ranges::copy(std::string_view{"null"}, ser.out());
    return ser.out();
  }

  template <typename OutputIt, str_c Str>
  OutputIt tag_invoke(tag_t<serde::serialize>, serializer<OutputIt> & ser, Str const& value)
  {
    if constexpr(meta::is_template_instance_of(^^Str, ^^std::array))
    {
      return std::format_to(
          ser.out(), "\"{}\"",
          std::string_view{value.data(), ::strnlen(value.data(), value.size())});
    }
    else
    {
      return std::format_to(ser.out(), "\"{}\"", value);
    }
  }

  template <typename OutputIt, number_c Num>
  OutputIt tag_invoke(tag_t<serde::serialize>, serializer<OutputIt> & ser, Num const& value)
  {
    return std::format_to(ser.out(), "{}", value);
  }

  template <typename OutputIt>
  OutputIt tag_invoke(tag_t<serde::serialize>, serializer<OutputIt> & ser, char value)
  {
    auto& out = ser.out();
    out++     = '"';
    out++     = value;
    out++     = '"';
    return out;
  }

  template <typename OutputIt>
  OutputIt tag_invoke(tag_t<serde::serialize>, serializer<OutputIt> & ser, boolean const& value)
  {
    std::ranges::copy(std::string_view{value ? "true" : "false"}, ser.out());
    return ser.out();
  }

  template <typename OutputIt, derives_c<derive_t<Format>> T>
  OutputIt tag_invoke(tag_t<serde::serialize>, serializer<OutputIt> & ser, T const& value)
  {
    return std::format_to(ser.out(), "\"{}\"", value);
  }

  template <typename OutputIt, seq_c Seq>
  OutputIt tag_invoke(tag_t<serde::serialize>, serializer<OutputIt> & ser, Seq const& value)
  {
    auto& out = ser.out();
    out++     = '[';
    if(value.empty())
    {
      out++ = ']';
      return out;
    }

    auto view = std::views::all(value);

    for(const auto& elem : view | std::views::take(value.size() - 1))
    {
      serialize(ser, elem);
      out++ = ',';
    }
    serialize(ser, view.back());

    out++ = ']';
    return out;
  }

  template <typename OutputIt, pair_c Pair>
  OutputIt tag_invoke(tag_t<serde::serialize>, serializer<OutputIt> & ser, Pair const& value)
  {
    auto& out = ser.out();
    out++     = '{';
    serialize(ser, value.first);
    out++ = ':';
    reflex::visit([&ser](const auto& v) { serialize(ser, v); }, value.second);
    out++ = '}';
    return out;
  }

  template <typename OutputIt, map_c Map>
  OutputIt tag_invoke(tag_t<serde::serialize>, serializer<OutputIt> & ser, Map const& value)
  {
    auto& out = ser.out();
    out++     = '{';

    bool first = true;

    for(const auto& [key, val] : value)
    {
      if(not first)
      {
        out++ = ',';
      }
      else
      {
        first = false;
      }
      serialize(ser, key);
      out++ = ':';
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
    out++ = '}';
    return out;
  }

  template <typename OutputIt, aggregate_c Agg, typename TagT = std::false_type>
    requires(not(str_c<Agg> or seq_c<Agg>)) // std::array<char> may be considered as an aggregate
  OutputIt tag_invoke(
      tag_t<serde::serialize>, serializer<OutputIt> & ser, Agg const& value, TagT = {})
  {
    auto& out = ser.out();
    out++     = '{';

    constexpr bool tag = TagT::value;

    bool first = not tag;

    if constexpr(tag)
    {
      serialize(ser, "__type");
      out++                   = ':';
      static auto expected_id = std::hash<std::string_view>{}(identifier_of(dealias(decay(^^Agg))));
      serialize(ser, expected_id);
    }

    static constexpr auto type = decay(type_of(^^value));
    template for(constexpr auto member : define_static_array(
                     nonstatic_data_members_of(type, std::meta::access_context::current())))
    {
      constexpr std::string_view name = serialized_name(member);
      if(not first)
      {
        out++ = ',';
      }
      else
      {
        first = false;
      }
      serialize(ser, name);
      out++                    = ':';
      auto const& member_value = value.[:member:];
      reflex::visit([&ser](const auto& v) { serialize(ser, v); }, member_value);
    }
    out++ = '}';
    return out;
  }

  template <typename OutputIt, visitable_c T>
  OutputIt tag_invoke(tag_t<serde::serialize>, serializer<OutputIt> & ser, T const& value)
  {
    return visit([&ser](const auto& v) { return serialize(ser, v); }, value);
  }

  template <std::input_iterator InputIt> class deserializer
  {
  public:
    using range_cursor = std::ranges::subrange<InputIt, InputIt>;

  private:
    range_cursor cursor_;

  public:
    bool at_end() const
    {
      return cursor_.empty();
    }

    InputIt begin() const
    {
      return cursor_.begin();
    }

    InputIt end() const
    {
      return cursor_.end();
    }

    InputIt advance_while(auto pred)
    {
      while(not at_end() and pred(peek()))
      {
        advance();
      }
      return cursor_.begin();
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

    deserializer(InputIt begin, InputIt end) : cursor_{begin, end}
    {}

    template <typename T>
      requires requires(T const& v) { v.view(); }
    deserializer(T const& v) : cursor_{v.view().begin(), v.view().end()}
    {}

    template <typename T>
      requires requires(T const& v) {
        v.begin();
        v.end();
      }
    deserializer(T const& v) : cursor_{v.begin(), v.end()}
    {}

    template <typename T>
      requires requires(T& v) {
        InputIt{v};
        InputIt{};
      }
    deserializer(T& v) : cursor_{InputIt{v}, InputIt{}}
    {}

    template <typename T = json::value> T load()
    {
      return deserialize(*this, std::type_identity<T>{});
    }
  };

  template <typename... TArgs>
  deserializer(std::basic_string<TArgs...> const& in)
      -> deserializer<typename std::basic_string<TArgs...>::const_iterator>;

  template <typename... TArgs>
  deserializer(std::basic_string_view<TArgs...> const& in)
      -> deserializer<typename std::basic_string_view<TArgs...>::const_iterator>;

  template <typename CharT, typename CharTrait = std::char_traits<CharT>>
  deserializer(std::basic_istream<CharT, CharTrait>)
      -> deserializer<std::istreambuf_iterator<CharT>>;

  template <typename It> auto tag_invoke(tag_t<serde::deserialize>, deserializer<It> & de)
  {
    return deserialize(de, std::type_identity<json::value>{});
  }

  template <typename It>
  auto tag_invoke(
      tag_t<serde::deserialize>, deserializer<It> & de, std::type_identity<json::null_t>)
  {
    de.expect("null");
    return null;
  }

  template <typename It, str_c Str>
  auto tag_invoke(tag_t<serde::deserialize>, deserializer<It> & de, std::type_identity<Str>)
  {
    if(de.advance() != '"')
    {
      throw std::runtime_error("Expected '\"' at start of JSON string");
    }

    Str value;

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
            throw std::runtime_error("\\uXXXX escapes not implemented");
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

  template <typename It, number_c Num>
  auto tag_invoke(tag_t<serde::deserialize>, deserializer<It> & de, std::type_identity<Num>)
  {
    Num value;
    if constexpr(
        std::contiguous_iterator<It>
        and std::same_as<std::remove_cv_t<std::iter_value_t<It>>, char>)
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

  template <typename It>
  auto tag_invoke(tag_t<serde::deserialize>, deserializer<It> & de, std::type_identity<boolean>)
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

  template <typename It, derives_c<derive_t<Parse>> T>
  auto tag_invoke(tag_t<serde::deserialize>, deserializer<It> & de, std::type_identity<T>)
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

  template <typename It, seq_c Seq>
  auto tag_invoke(tag_t<serde::deserialize>, deserializer<It> & de, std::type_identity<Seq>)
  {
    if(de.advance() != '[')
      throw std::runtime_error("Expected '[' at start of JSON array");

    Seq value;

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

  template <typename It, object_visitable_c Map>
    requires(
        not(meta::is_template_instance_of(^^Map, ^^poly::var)
            // std::array<char> may be considered as a visitable object
            or str_c<Map>
            or seq_c<Map>))
  auto tag_invoke(tag_t<serde::deserialize>, deserializer<It> & de, std::type_identity<Map>)
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

  template <typename It, typename var_type>
    requires(meta::is_template_instance_of(^^var_type, ^^poly::var))
  auto tag_invoke(tag_t<serde::deserialize>, deserializer<It> & de, std::type_identity<var_type>)
      -> var_type
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
} // namespace reflex::serde::json

REFLEX_EXPORT namespace reflex::serde::ser
{
  constexpr auto json = ^^reflex::serde::json::serializer;
}

REFLEX_EXPORT namespace reflex::serde::de
{
  constexpr auto json = ^^reflex::serde::json::deserializer;
}
