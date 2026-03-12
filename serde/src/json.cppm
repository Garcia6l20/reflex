export module reflex.serde.json;

export import reflex.serde;

export import :value;

import std;

namespace reflex::serde::json
{
class serializer
{
public:
  template <typename Out> constexpr Out& operator()(Out& out, null_t const&) const
  {
    out << "null";
    return out;
  }

  template <typename Out> constexpr Out& operator()(Out& out, detail::str_c auto const& str) const
  {
    out << '"' << std::string_view(str) << '"';
    return out;
  }

  template <typename Out> constexpr Out& operator()(Out& out, detail::number_c auto num) const
  {
    out << num;
    return out;
  }

  template <typename Out> constexpr Out& operator()(Out& out, boolean b) const
  {
    out << (b ? "true" : "false");
    return out;
  }

  template <typename Out> constexpr Out& operator()(Out& out, detail::seq_c auto const& arr) const
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

  template <typename Out> constexpr Out& operator()(Out& out, detail::pair_c auto const& p) const
  {
    out << '{';
    (*this)(out, p.first);
    out << ':';
    reflex::visit([this, &out](const auto& val) { (*this)(out, val); }, p.second);
    out << '}';
    return out;
  }

  template <typename Out> constexpr Out& operator()(Out& out, detail::map_c auto const& obj) const
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

    template for(constexpr auto member : define_static_array(
                     nonstatic_data_members_of(decay(^^decltype(val)),
                                               std::meta::access_context::current())))
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
  template <typename T, typename Iter>
  constexpr auto load(Iter& begin, Iter const& end) const
    requires(detail::is_base_value_type(^^T) or std::constructible_from<value, T>)
  {
    const auto skip_ws = [&](Iter& it)
    {
      while(it != end && is_space(static_cast<unsigned char>(*it)))
        ++it;
    };

    if constexpr(std::same_as<T, json::string>)
    {
      if(begin == end || *begin != '"')
        throw std::runtime_error("Expected '\"' at start of JSON string");

      ++begin; // consume opening quote
      json::string out;
      while(begin != end)
      {
        char c = *begin++;
        if(c == '"') // closing quote
          return out;

        if(c == '\\') // escape
        {
          if(begin == end)
            throw std::runtime_error("Invalid escape at end of input");
          char esc = *begin++;
          switch(esc)
          {
            case '"':
              out.push_back('"');
              break;
            case '\\':
              out.push_back('\\');
              break;
            case '/':
              out.push_back('/');
              break;
            case 'b':
              out.push_back('\b');
              break;
            case 'f':
              out.push_back('\f');
              break;
            case 'n':
              out.push_back('\n');
              break;
            case 'r':
              out.push_back('\r');
              break;
            case 't':
              out.push_back('\t');
              break;
            case 'u':
              throw std::runtime_error("\\uXXXX escapes not implemented");
            default:
              throw std::runtime_error(std::format("Unknown escape: \\{}", esc));
          }
        }
        else
        {
          out.push_back(c);
        }
      }
      throw std::runtime_error("Unterminated JSON string");
    }
    else if constexpr(detail::number_c<T>)
    {
      // Collect number characters into a temporary string, then convert.
      if(begin == end)
        throw std::runtime_error("Unexpected end when parsing number");

      Iter it = begin;
      // A permissive set of number characters: sign, digits, decimal point, exponent.
      static const auto is_num_char = [](char ch)
      {
        return (ch >= '0' && ch <= '9')
            || ch
            == '+'
            || ch
            == '-'
            || ch
            == '.'
            || ch
            == 'e'
            || ch
            == 'E';
      };

      // Accept optional leading sign
      if(it != end && (*it == '+' || *it == '-'))
      {
        ++it;
      }

      bool any = false;
      while(it != end && is_num_char(*it))
      {
        any = true;
        ++it;
      }
      if(!any)
        throw std::runtime_error("Invalid number");

      T val{};
      auto [ptr, ec] = std::from_chars(&*begin, &*end, val);
      if(ec != std::errc{})
      {
        throw std::runtime_error("Failed to parse number");
      }
      begin = ptr;

      return val;
    }
    else if constexpr(std::same_as<T, json::array>)
    {
      if(begin == end || *begin != '[')
        throw std::runtime_error("Expected '[' at start of JSON array");
      ++begin; // consume '['
      json::array out;
      skip_ws(begin);
      if(begin != end && *begin == ']')
      {
        ++begin;
        return out;
      } // empty array
      while(true)
      {
        skip_ws(begin);
        // parse any value using the dispatcher
        out.push_back(load(begin, end));
        skip_ws(begin);
        if(begin == end)
          throw std::runtime_error("Unterminated array");
        if(*begin == ',')
        {
          ++begin;
          continue;
        }
        if(*begin == ']')
        {
          ++begin;
          break;
        }
        throw std::runtime_error("Expected ',' or ']' in array");
      }
      return out;
    }
    else if constexpr(std::same_as<T, json::object>)
    {
      if(begin == end || *begin != '{')
        throw std::runtime_error("Expected '{' at start of JSON object");
      ++begin; // consume '{'
      json::object out;
      skip_ws(begin);
      if(begin != end && *begin == '}')
      {
        ++begin;
        return out;
      } // empty object
      while(true)
      {
        skip_ws(begin);
        // key must be a string
        json::string key = load<json::string, Iter>(begin, end);
        skip_ws(begin);
        if(begin == end || *begin != ':')
          throw std::runtime_error("Expected ':' after object key");
        ++begin; // consume ':'
        skip_ws(begin);
        json::value val = load(begin, end);
        out.emplace(std::move(key), std::move(val));
        skip_ws(begin);
        if(begin == end)
          throw std::runtime_error("Unterminated object");
        if(*begin == ',')
        {
          ++begin;
          continue;
        }
        if(*begin == '}')
        {
          ++begin;
          break;
        }
        throw std::runtime_error("Expected ',' or '}' in object");
      }
      return out;
    }
    else
    {
      // fallback: unsupported T
      throw std::runtime_error("Type not supported for deserialization (requested load<T>)");
    }
  }

  template <typename T, typename Iter>
  auto load(Iter& begin, Iter const& end) const
    requires(is_aggregate_type(^^T))
  {
    const auto skip_ws = [&](Iter& it)
    {
      while(it != end && is_space(static_cast<unsigned char>(*it)))
        ++it;
    };
    if(begin == end || *begin != '{')
      throw std::runtime_error("Expected '{' at start of JSON object for aggregate");
    ++begin; // consume '{'
    T out{};
    skip_ws(begin);
    if(begin != end && *begin == '}')
    {
      ++begin;
      return out;
    } // empty object

    template for(constexpr auto member : define_static_array(
                     nonstatic_data_members_of(^^T, std::meta::access_context::current())))
    {
      constexpr std::string_view name = serialized_name(member);
      // parse key
      json::string key = load<json::string, Iter>(begin, end);
      if(key != name)
        throw std::runtime_error(std::format("Expected key '{}' in JSON object", name));
      skip_ws(begin);
      if(begin == end || *begin != ':')
        throw std::runtime_error("Expected ':' after object key in aggregate");
      ++begin; // consume ':'
      skip_ws(begin);
      // parse value
      constexpr auto member_type = type_of(member);
      using MemberType           = [:member_type:];
      out.[:member:]             = load<MemberType>(begin, end);
      skip_ws(begin);
      if(begin == end)
        throw std::runtime_error("Unterminated object in aggregate");
      if(*begin == ',')
      {
        ++begin;
        continue;
      }
      if(*begin == '}')
      {
        ++begin;
        break;
      }
      throw std::runtime_error("Expected ',' or '}' in object for aggregate");
    }
    return out;
  }

  value load(auto& begin, auto const& end) const
  {
    while(begin != end && std::isspace(*begin))
    {
      ++begin;
    }
    if(begin == end)
    {
      throw std::runtime_error("Unexpected end of input");
    }
    switch(*begin)
    {
      case 't': // assuming true
        begin += 4;
        return true;
      case 'f': // assuming false
        begin += 5;
        return false;
      case 'n': // assuming null
        begin += 4;
        return null;
      case '{':
        return load<json::object>(begin, end);
      case '[':
        return load<json::array>(begin, end);
      case '"':
        return load<json::string>(begin, end);
      default:
        return load<json::number>(begin, end);
    }
    throw std::runtime_error("Error occurred while parsing value");
  }

public:
  template <typename T> T load(auto&& in) const
  {
    const auto view  = in.view();
    auto       begin = view.begin();
    const auto end   = view.end();
    return load<T>(begin, end);
  }

  value load(auto&& in) const
  {
    const auto view  = in.view();
    auto       begin = view.begin();
    const auto end   = view.end();
    return load(begin, end);
  }

  value operator()(auto&& in) const
  {
    return load(in);
  }
};

} // namespace reflex::serde::json
