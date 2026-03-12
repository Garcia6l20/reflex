export module reflex.serde.json;

export import reflex.serde;

import std;

namespace reflex::serde::json
{
namespace detail
{

template <typename T>
concept str_c = decays_to_c<T, char*>
             or decays_to_c<T, std::string>
             or decays_to_c<T, std::string_view>
             or requires(T s) { std::string_view(s); };

template <typename T>
concept map_c = requires(T t) {
  { t.begin() } -> std::input_iterator;
  { t.end() } -> std::sentinel_for<decltype(t.begin())>;
  typename T::value_type;
  typename T::key_type;
  typename T::mapped_type;
};

template <typename T>
concept seq_c = requires(T t) {
  { std::begin(t) } -> std::input_iterator;
  { std::end(t) } -> std::sentinel_for<decltype(std::begin(t))>;
  typename T::value_type;
} and not map_c<T> and not str_c<T>;

template <typename T>
concept pair_c = requires(T t) {
  typename T::first_type;
  typename T::second_type;
};

template <typename T>
concept number_c = (std::integral<T> or std::floating_point<T>) and not decays_to_c<T, bool>;

static_assert(seq_c<std::vector<int>>);
static_assert(map_c<std::unordered_map<int, int>>);
static_assert(not seq_c<std::unordered_map<int, int>>);

} // namespace detail
} // namespace reflex::serde::json

export namespace reflex::serde::json
{
using null    = std::monostate;
using string  = std::string;
using number  = double;
using boolean = bool;

struct value;

struct object : std::unordered_map<string, value>
{
  using std::unordered_map<string, value>::unordered_map;
};

struct array : std::vector<value>
{
  using std::vector<value>::vector;
};

namespace detail
{
using base_value = std::variant<null, string, number, boolean, array, object>;

constexpr auto base_value_types =
    define_static_array(std::array{^^null, ^^string, ^^number, ^^boolean, ^^array, ^^object}
                        | std::views::transform(std::meta::dealias));

consteval bool is_base_value_type(meta::info T)
{
  return std::ranges::contains(base_value_types, dealias(T));
}

static_assert(is_base_value_type(^^null));
static_assert(is_base_value_type(^^std::string));
static_assert(is_base_value_type(^^number));
static_assert(is_base_value_type(^^boolean));
static_assert(is_base_value_type(^^array));
static_assert(is_base_value_type(^^object));

} // namespace detail

struct value : detail::base_value
{
  using detail::base_value::base_value;

  value() = default;

  value(detail::number_c auto n)
    requires(not std::constructible_from<detail::base_value, decltype(n)>)
      : detail::base_value(number(n))
  {
  }

  constexpr bool is_null() const
  {
    return std::holds_alternative<null>(*this);
  }

  constexpr bool is_string() const
  {
    return std::holds_alternative<string>(*this);
  }

  template <typename T> constexpr auto const& as() const
  {
    return std::get<T>(*this);
  }

  constexpr bool is_number() const
  {
    return std::holds_alternative<number>(*this);
  }

  constexpr bool is_boolean() const
  {
    return std::holds_alternative<boolean>(*this);
  }

  constexpr bool is_array() const
  {
    return std::holds_alternative<array>(*this);
  }

  constexpr bool is_object() const
  {
    return std::holds_alternative<object>(*this);
  }

  constexpr bool operator==(null const&) const
  {
    return is_null();
  }

  constexpr bool operator==(detail::str_c auto const& s) const
  {
    return is_string() && as<string>() == s;
  }

  constexpr bool operator==(detail::number_c auto num) const
  {
    return is_number() && as<number>() == num;
  }

  constexpr bool operator==(boolean b) const
  {
    return is_boolean() && as<boolean>() == b;
  }

  constexpr bool operator==(detail::seq_c auto const& arr) const
  {
    return is_array() && as<array>() == arr;
  }

  constexpr bool operator==(detail::map_c auto const& obj) const
  {
    return is_object() && as<object>() == obj;
  }
};

class serializer
{
public:
  template <typename Out> constexpr Out& operator()(Out& out, null) const
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
        return null{};
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
