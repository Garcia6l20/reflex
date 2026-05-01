#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <reflex/parse.hpp>
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

  class serializer
  {
  public:
    template <typename Out, typename T, bool tag = false>
    constexpr Out& operator()(Out& out, T const& value) const
    {
      if constexpr(decays_to_c<T, null_t>)
      {
        out << "null";
      }
      else if constexpr(decays_to_c<T, std::byte>)
      {
        out << std::to_integer<unsigned int>(value);
      }
      else if constexpr(str_c<T>)
      {
        out << '"' << std::string_view(value) << '"';
      }
      else if constexpr(number_c<T>)
      {
        std::format_to(std::ostreambuf_iterator<typename Out::char_type>(out), "{}", value);
      }
      else if constexpr(std::same_as<std::remove_cvref_t<T>, boolean>)
      {
        out << (value ? "true" : "false");
      }
      else if constexpr(seq_c<T>)
      {
        out << '[';
        if(value.empty())
        {
          out << ']';
          return out;
        }

        auto view = std::views::all(value);

        for(const auto& elem : view | std::views::take(value.size() - 1))
        {
          (*this)(out, elem);
          out << ',';
        }
        (*this)(out, view.back());

        out << ']';
        return out;
      }
      else if constexpr(pair_c<T>)
      {
        out << '{';
        (*this)(out, value.first);
        out << ':';
        reflex::visit([this, &out](const auto& v) { (*this)(out, v); }, value.second);
        out << '}';
      }
      else if constexpr(map_c<T>)
      {
        out << '{';
        if(value.empty())
        {
          out << '}';
          return out;
        }

        bool first = true;

        for(const auto& [key, val] : value)
        {
          if(not first)
          {
            out << ',';
          }
          else
          {
            first = false;
          }
          (*this)(out, key);
          out << ':';
          reflex::visit(
              [this, &out]<typename U>(const U& v) {
                using object_type = std::remove_cvref_t<T>;
                using var_type    = typename object_type::mapped_type;
                if constexpr(std::ranges::contains(detail::aggregate_types_of_var<var_type>(), ^^U))
                {
                  (*this).template operator()<Out, U, true>(out, v);
                }
                else
                {
                  (*this)(out, v);
                }
              },
              val);
        }
        out << '}';
      }
      else if constexpr(aggregate_c<T>)
      {
        out << '{';

        bool first = not tag;

        if constexpr(tag)
        {
          (*this)(out, "__type");
          out << ':';
          static auto expected_id =
              std::hash<std::string_view>{}(identifier_of(dealias(decay(^^T))));
          (*this)(out, expected_id);
        }

        static constexpr auto type = decay(type_of(^^value));
        template for(constexpr auto member : define_static_array(
                         nonstatic_data_members_of(type, std::meta::access_context::current())))
        {
          constexpr std::string_view name = reflex::serde::serialized_name(member);
          if(not first)
          {
            out << ',';
          }
          else
          {
            first = false;
          }
          (*this)(out, name);
          out << ':';
          auto const& member_value = value.[:member:];
          reflex::visit([this, &out](const auto& v) { (*this)(out, v); }, member_value);
        }
        out << '}';
      }
      else if constexpr(visitable_c<T>)
      {
        reflex::visit([&](const auto& v) { (*this)(out, v); }, value);
      }
      else if constexpr(std::formattable<T, typename Out::char_type>)
      {
        // Fallback for types that are formattable but not directly serializable
        std::format_to(std::ostreambuf_iterator<typename Out::char_type>(out), "\"{}\"", value);
      }
      else
      {
        static_assert(false, "Type is not serializable to JSON");
      }
      return out;
    }
  };

  class deserializer
  {
  private:
    template <std::input_iterator It, std::sentinel_for<It> End>
    using range_cursor = std::ranges::subrange<It, End>;

    template <typename R> static bool at_end(R const& r)
    {
      return r.empty();
    }

    template <typename R> static char peek(R const& r)
    {
      if(at_end(r))
        throw std::runtime_error("Unexpected end of JSON input");
      return static_cast<char>(*r.begin());
    }

    template <typename R> static char advance(R& r)
    {
      const char c = peek(r);
      r.advance(1);
      return c;
    }

    template <typename R> static void expect(R& r, std::string_view token)
    {
      for(char expected : token)
      {
        if(advance(r) != expected)
        {
          throw std::runtime_error(std::format("Expected '{}'", token));
        }
      }
    }

    template <typename R> static void ltrim(R& r)
    {
      while(!at_end(r))
      {
        const unsigned char ch = static_cast<unsigned char>(peek(r));
        if(std::isspace(ch))
        {
          advance(r);
        }
        else
        {
          break;
        }
      }
    }

    template <typename R> static constexpr void load_into(R& r, str_c auto& value)
    {
      if(advance(r) != '"')
      {
        throw std::runtime_error("Expected '\"' at start of JSON string");
      }

      while(!at_end(r))
      {
        const char c = advance(r);
        if(c == '"')
          return;

        if(c == '\\')
        {
          const char esc = advance(r);
          switch(esc)
          {
            case '"':
              value.push_back('"');
              break;
            case '\\':
              value.push_back('\\');
              break;
            case '/':
              value.push_back('/');
              break;
            case 'b':
              value.push_back('\b');
              break;
            case 'f':
              value.push_back('\f');
              break;
            case 'n':
              value.push_back('\n');
              break;
            case 'r':
              value.push_back('\r');
              break;
            case 't':
              value.push_back('\t');
              break;
            case 'u':
              throw std::runtime_error("\\uXXXX escapes not implemented");
            default:
              throw std::runtime_error(std::format("Unknown escape: \\{}", esc));
          }
        }
        else
        {
          value.push_back(c);
        }
      }
      throw std::runtime_error("Unterminated JSON string");
    }

    template <typename R> static constexpr void load_into(R& r, bool& value)
    {
      if(peek(r) == 't')
      {
        expect(r, "true");
        value = true;
        return;
      }
      if(peek(r) == 'f')
      {
        expect(r, "false");
        value = false;
        return;
      }
      throw std::runtime_error("Expected 'true' or 'false'");
    }

    template <typename R> static constexpr void load_into(R& r, json::null_t&)
    {
      expect(r, "null");
    }

    template <typename R> static constexpr void load_into(R& r, number_c auto& value)
    {
      static const auto is_num_char = [](char ch) {
        return std::isdigit(static_cast<unsigned char>(ch))
            or (ch == '+')
            or (ch == '-')
            or (ch == '.')
            or (ch == 'e')
            or (ch == 'E');
      };

      std::inplace_vector<char, 64> token;
      while(!at_end(r) and is_num_char(peek(r)))
      {
        token.push_back(advance(r));
      }

      if(token.empty())
      {
        throw std::runtime_error("Failed to parse number");
      }

      auto [ptr, ec] = std::from_chars(token.data(), token.data() + token.size(), value);
      if(ec != std::errc{} or ptr != token.data() + token.size())
      {
        throw std::runtime_error("Failed to parse number");
      }
    }

    template <typename R> static constexpr void load_into(R& r, seq_c auto& value)
    {
      if(advance(r) != '[')
        throw std::runtime_error("Expected '[' at start of JSON array");

      ltrim(r);
      if(!at_end(r) and peek(r) == ']')
      {
        advance(r);
        return;
      }

      while(true)
      {
        typename std::remove_cvref_t<decltype(value)>::value_type elem;
        load_into(r, elem);
        value.push_back(std::move(elem));

        ltrim(r);
        const char sep = advance(r);
        if(sep == ',')
        {
          ltrim(r);
          continue;
        }
        if(sep == ']')
        {
          break;
        }
        throw std::runtime_error("Expected ',' or ']' in array");
      }
    }

    template <typename T, typename R> struct try_load_result
    {
      std::optional<T> value;
      R                r;
      std::string_view error_message;

      explicit operator bool() const noexcept
      {
        return value.has_value() and error_message.empty();
      }
    };

    template <aggregate_c Obj, typename R> static constexpr try_load_result<Obj, R> try_load(R r)
    {
      if(advance(r) != '{')
      {
        return {
            .value = std::nullopt, .r = r, .error_message = "Expected '{' at start of JSON object"};
      }

      ltrim(r);
      if(!at_end(r) and peek(r) == '}')
      {
        advance(r);
        return {.value = std::nullopt, .r = r, .error_message = ""};
      }

      {
        // lookup for __type tag
        std::string key;
        load_into(r, key);
        ltrim(r);
        if(advance(r) != ':')
        {
          return {.value = std::nullopt, .r = r, .error_message = "Expected ':' after object key"};
        }
        if(key == "__type")
        {
          std::size_t id;
          load_into(r, id);
          static auto expected_id =
              std::hash<std::string_view>{}(identifier_of(dealias(decay(^^Obj))));
          if(id != expected_id)
          {
            return {.value = std::nullopt, .r = r, .error_message = "Type tag mismatch"};
          }

          ltrim(r);
          const char sep = advance(r);
          if(sep != ',')
          {
            return {
                .value = std::nullopt, .r = r, .error_message = "Expected ',' after __type tag"};
          }
        }
        else
        {
          return {.value = std::nullopt, .r = r, .error_message = "Missing __type tag"};
        }
      }

      Obj obj{};
      while(true)
      {
      __next:
        ltrim(r);
        std::string key;
        load_into(r, key);

        template for(constexpr auto member : define_static_array(
                         nonstatic_data_members_of(^^Obj, std::meta::access_context::current())))
        {
          constexpr std::string_view name = reflex::serde::serialized_name(member);
          if(name != key)
          {
            continue;
          }
          ltrim(r);
          if(advance(r) != ':')
          {
            return {
                .value = std::nullopt, .r = r, .error_message = "Expected ':' after object key"};
          }

          ltrim(r);
          load_into(r, obj.[:member:]);

          ltrim(r);
          const char sep = advance(r);
          if(sep == ',')
          {
            goto __next;
          }
          if(sep == '}')
          {
            return {.value = std::move(obj), .r = r, .error_message = ""};
          }
          return {.value = std::nullopt, .r = r, .error_message = "Expected ',' or '}' in object"};
        }
        return {.value = std::nullopt, .r = r, .error_message = "Unexpected key in object: " + key};
      }
      return {.value = std::nullopt, .r = r, .error_message = "Non-matching aggregate type"};
    }

    template <typename R, object_visitable_c Map>
      requires(not meta::is_template_instance_of(^^Map, ^^poly::var))
    static constexpr void load_into(R& r, Map& value)
    {
      if(advance(r) != '{')
        throw std::runtime_error("Expected '{' at start of JSON object");

      ltrim(r);
      if(!at_end(r) and peek(r) == '}')
      {
        advance(r);
        return;
      }

      while(true)
      {
        ltrim(r);
        std::string key;
        load_into(r, key);

        ltrim(r);
        if(advance(r) != ':')
        {
          throw std::runtime_error("Expected ':' after object key");
        }

        ltrim(r);
        serde::object_visit(key, value, [&](auto&& v) { load_into(r, v); });

        ltrim(r);
        const char sep = advance(r);
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
    }

    template <typename R, typename var_type>
      requires(meta::is_template_instance_of(^^var_type, ^^poly::var))
    static constexpr void load_into(R& r, var_type& value)
    {
      ltrim(r);
      switch(peek(r))
      {
        case 't':
          expect(r, "true");
          value = true;
          return;
        case 'f':
          expect(r, "false");
          value = false;
          return;
        case 'n':
          expect(r, "null");
          value = var_type(null);
          return;
        case '{':
        {
          template for(constexpr auto a : detail::aggregate_types_of_var<var_type>())
          {
            using T     = [:a:];
            auto result = try_load<T>(r);
            if(!result)
            {
              continue;
            }
            value = std::move(*result.value);
            r     = std::move(result.r);
            return;
          }

          // no aggregate type matched, load as generic object
          auto& obj = value.template emplace<typename var_type::obj_type>();
          load_into(r, obj);

          return;
        }
        case '[':
          load_into(r, value.template emplace<typename var_type::arr_type>());
          return;
        case '"':
          load_into(r, value.template emplace<json::string>());
          return;
        default:
          load_into(r, value.template emplace<json::number>());
          return;
      }
    }

    // Fallback for types that are not directly deserializable but parsable
    template <typename R, parsable_c T>
      requires(not serializable_c<T>)
    static void load_into(R& r, T& value)
    {
      std::string token;
      load_into(r, token);
      auto result = parse<std::remove_cvref_t<T>>(token);
      if(!result)
      {
        throw std::runtime_error(
            std::format("Failed to parse value: {}", result.error().message()));
      }
      value = std::move(result).value();
    }

  public:
    deserializer() = default;

    template <typename T = json::value, std::input_iterator It, std::sentinel_for<It> End>
    static T load(It first, End last)
    {
      range_cursor<It, End> input{first, last};
      T                     value{};
      ltrim(input);
      load_into(input, value);
      ltrim(input);
      if(!at_end(input))
      {
        throw std::runtime_error("Unexpected trailing JSON input");
      }
      return value;
    }

    template <typename T = json::value> static T load(str_c auto&& in)
    {
      std::string_view view{in};
      return load<T>(view.begin(), view.end());
    }

    template <typename T = json::value>
    static T load(auto&& in)
      requires requires() { in.view(); }
    {
      auto view = in.view();
      return load<T>(view.begin(), view.end());
    }

    template <typename T = json::value, std::input_iterator It, std::sentinel_for<It> End>
    static T operator()(It first, End last)
    {
      return load<T>(first, last);
    }

    static auto operator()(auto&& in)
    {
      return load(in);
    }
  };
} // namespace reflex::serde::json

REFLEX_EXPORT namespace reflex::serde::ser
{
  using json = reflex::serde::json::serializer;
}

REFLEX_EXPORT namespace reflex::serde::de
{
  using json = reflex::serde::json::deserializer;
}
