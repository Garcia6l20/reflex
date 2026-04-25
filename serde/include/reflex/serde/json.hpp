#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <reflex/serde/json_value.hpp>
#endif

REFLEX_EXPORT namespace reflex::serde::json
{
  class serializer
  {
  public:
    template <typename Out> constexpr Out& operator()(Out& out, null_t const&) const
    {
      out << "null";
      return out;
    }

    template <typename Out> constexpr Out& operator()(Out& out, str_c auto const& str) const
    {
      out << '"' << std::string_view(str) << '"';
      return out;
    }

    template <typename Out> constexpr Out& operator()(Out& out, number_c auto num) const
    {
      out << num;
      return out;
    }

    template <typename Out> constexpr Out& operator()(Out& out, std::byte num) const
    {
      out << std::to_integer<unsigned int>(num);
      return out;
    }

    template <typename Out> constexpr Out& operator()(Out& out, boolean b) const
    {
      out << (b ? "true" : "false");
      return out;
    }

    template <typename Out> constexpr Out& operator()(Out& out, seq_c auto const& arr) const
    {
      out << '[';
      if(arr.empty())
      {
        out << ']';
        return out;
      }

      auto view = std::views::all(arr);

      for(const auto& elem : view | std::views::take(arr.size() - 1))
      {
        (*this)(out, elem);
        out << ',';
      }
      (*this)(out, view.back());

      out << ']';
      return out;
    }

    template <typename Out> constexpr Out& operator()(Out& out, pair_c auto const& p) const
    {
      out << '{';
      (*this)(out, p.first);
      out << ':';
      reflex::visit([this, &out](const auto& val) { (*this)(out, val); }, p.second);
      out << '}';
      return out;
    }

    template <typename Out> constexpr Out& operator()(Out& out, map_c auto const& obj) const
    {
      out << '{';
      if(obj.empty())
      {
        out << '}';
        return out;
      }

      auto view = std::views::all(obj);

      for(const auto& [key, val] : view | std::views::take(obj.size() - 1))
      {
        (*this)(out, key);
        out << ':';
        reflex::visit([this, &out](const auto& value) { (*this)(out, value); }, val);
        out << ',';
      }

      const auto& [last_key, last_val] = view.back();
      (*this)(out, last_key);
      out << ':';
      reflex::visit([this, &out](const auto& value) { (*this)(out, value); }, last_val);
      out << '}';
      return out;
    }

    template <typename Out, aggregate_c T>
      requires(not seq_c<T> and not pair_c<T> and not map_c<T>)
    constexpr Out& operator()(Out& out, T const& val) const
    {
      out << '{';

      bool first = true;

      static constexpr auto type = decay(type_of(^^val));
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
        auto const& member_value = val.[:member:];
        reflex::visit([this, &out](const auto& value) { (*this)(out, value); }, member_value);
      }
      out << '}';
      return out;
    }

    template <typename Out, visitable_c T>
      requires(not seq_c<T> and not pair_c<T> and not map_c<T> and not aggregate_c<T>)
    constexpr Out& operator()(Out& out, T const& val) const
    {
      reflex::visit([&](const auto& v) { (*this)(out, v); }, val);
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
        json::value elem;
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

    template <typename R> static constexpr void load_into(R& r, json::value& value)
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
          value = null;
          return;
        case '{':
          load_into(r, value.template emplace<json::object>());
          return;
        case '[':
          load_into(r, value.template emplace<json::array>());
          return;
        case '"':
          load_into(r, value.template emplace<json::string>());
          return;
        default:
          load_into(r, value.template emplace<json::number>());
          return;
      }
    }

    template <typename R, object_visitable_c Map> static constexpr void load_into(R& r, Map& value)
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
