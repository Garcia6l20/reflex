#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#include <reflex/serde/json_value.hpp>

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

      std::size_t count = 0;
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

    template <typename Out> constexpr Out& operator()(Out& out, aggregate_c auto const& val) const
    {
      out << '{';

      bool first = true;

      template for(constexpr auto member : define_static_array(nonstatic_data_members_of(
                       decay(^^decltype(val)), std::meta::access_context::current())))
      {
        constexpr std::string_view name = serialized_name(member);
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
        auto& member_value = val.[:member:];
        reflex::visit([this, &out](const auto& value) { (*this)(out, value); }, member_value);
      }
      out << '}';
      return out;
    }
  };

  class deserializer
  {
  private:
    deserializer(std::string_view in) : lexme(in)
    {}

    std::string_view lexme;

    auto advance(std::size_t n = 1)
    {
      char c = lexme.front();
      lexme.remove_prefix(n);
      return c;
    }

    void ltrim()
    {
      lexme = reflex::ltrim(lexme);
    }

    constexpr void load_into(str_c auto& value)
    {
      if(lexme.empty() || lexme.front() != '"')
        throw std::runtime_error("Expected '\"' at start of JSON string");

      lexme.remove_prefix(1); // consume opening quote
      while(!lexme.empty())
      {
        char c = advance();
        if(c == '"') // closing quote
          return;

        if(c == '\\') // escape
        {
          if(lexme.empty())
          {
            throw std::runtime_error("Invalid escape at end of input");
          }
          char esc = advance();
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

    constexpr void load_into(bool& value)
    {
      if(lexme.size() >= 4 && lexme.substr(0, 4) == "true")
      {
        value = true;
        advance(4);
      }
      else if(lexme.size() >= 5 && lexme.substr(0, 5) == "false")
      {
        value = false;
        advance(5);
      }
      else
      {
        throw std::runtime_error("Expected 'true' or 'false'");
      }
    }

    constexpr void load_into(json::null_t& value)
    {
      if(lexme.size() >= 4 && lexme.substr(0, 4) == "null")
      {
        advance(4);
      }
      else
      {
        throw std::runtime_error("Expected 'null'");
      }
    }

    constexpr void load_into(number_c auto& value)
    {
      // Collect number characters into a temporary string, then convert.
      if(lexme.empty())
        throw std::runtime_error("Unexpected end when parsing number");

      // A permissive set of number characters: sign, digits, decimal point, exponent.
      static const auto is_num_char = [](char ch) {
        return (ch >= '0' && ch <= '9')
            or (ch == '+')
            or (ch == '-')
            or (ch == '.')
            or (ch == 'e')
            or (ch == 'E');
      };

      // Accept optional leading sign
      if(not lexme.empty() and (lexme.front() == '+' or lexme.front() == '-'))
      {
        advance();
      }

      auto begin     = lexme.data();
      auto end       = lexme.data() + lexme.size();
      auto [ptr, ec] = std::from_chars(begin, end, value);
      if(ec != std::errc{})
      {
        throw std::runtime_error("Failed to parse number");
      }
      advance(ptr - begin);
    }

    constexpr void load_into(seq_c auto& value)
    {
      if(lexme.empty() || lexme.front() != '[')
        throw std::runtime_error("Expected '[' at start of JSON array");
      advance(); // consume '['
      ltrim();
      if(!lexme.empty() && lexme.front() == ']')
      {
        advance(); // consume ']'
        return;
      } // empty array

      while(true)
      {
        json::value elem;
        load_into(elem);
        value.push_back(std::move(elem));
        ltrim();
        if(lexme.empty())
          throw std::runtime_error("Unterminated array");
        if(lexme.front() == ',')
        {
          advance(); // consume ','
          continue;
        }
        if(lexme.front() == ']')
        {
          advance(); // consume ']'
          break;
        }
        throw std::runtime_error("Expected ',' or ']' in array");
      }
    }

    constexpr void load_into(json::value& value)
    {
      ltrim();
      switch(lexme.front())
      {
        case 't': // assuming true
          advance(4);
          value = true;
          return;
        case 'f': // assuming false
          advance(5);
          value = false;
          return;
        case 'n': // assuming null
          advance(4);
          value = null;
          return;
        case '{':
        {
          load_into(value.template emplace<json::object>());
          return;
        }
        case '[':
        {
          load_into(value.template emplace<json::array>());
          return;
        }
        case '"':
        {
          load_into(value.template emplace<json::string>());
          return;
        }
        default:
        {
          load_into(value.template emplace<json::number>());
          return;
        }
      }
    }

    template <object_visitable_c Map> constexpr void load_into(Map& value)
    {
      if(lexme.empty() || lexme.front() != '{')
        throw std::runtime_error("Expected '{' at start of JSON object");
      advance(); // consume '{'
      ltrim();
      if(!lexme.empty() && lexme.front() == '}')
      {
        advance(); // consume '}'
        return;
      } // empty object
      while(true)
      {
        ltrim();
        // key must be a string
        std::string key;
        load_into(key);
        ltrim();
        if(lexme.empty() || lexme.front() != ':')
          throw std::runtime_error("Expected ':' after object key");
        advance(); // consume ':'
        ltrim();
        serde::object_visit(
            key, value,
            [this](auto&& v) { //
              load_into(v);
            });
        ltrim();
        if(lexme.empty())
          throw std::runtime_error("Unterminated object");
        if(lexme.front() == ',')
        {
          advance(); // consume ','
          continue;
        }
        if(lexme.front() == '}')
        {
          advance(); // consume '}'
          break;
        }
        throw std::runtime_error("Expected ',' or '}' in object");
      }
    }

  public:
    template <typename T = json::value> static T load(str_c auto&& in)
    {
      auto self = deserializer{in};
      T    value{};
      self.load_into(value);
      return value;
    }

    template <typename T = json::value>
    static T load(auto&& in)
      requires requires() { in.view(); }
    {
      return load<T>(in.view());
    }

    static auto operator()(auto&& in)
    {
      return load(in);
    }
  };

} // namespace reflex::serde::json
