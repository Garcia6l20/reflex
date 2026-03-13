export module reflex.serde.json:value;

import reflex.serde;

import std;

export namespace reflex::serde::json
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
struct null_t
{
  constexpr bool operator==(null_t const&) const noexcept
  {
    return true;
  }
};

constexpr null_t null{};

using string  = std::string;
using number  = double;
using boolean = bool;

struct value;

struct object : std::map<string, value>
{
  using std::map<string, value>::map;
};

struct array : std::vector<value>
{
  using std::vector<value>::vector;
};

namespace detail
{
using base_value = std::variant<null_t, string, number, boolean, array, object>;

constexpr auto base_value_types = define_static_array(
    std::array{^^null_t, ^^string, ^^number, ^^boolean, ^^array, ^^object}
    | std::views::transform(std::meta::dealias));

consteval bool is_base_value_type(meta::info T)
{
  return std::ranges::contains(base_value_types, dealias(T));
}

static_assert(is_base_value_type(^^null_t));
static_assert(is_base_value_type(^^std::string));
static_assert(is_base_value_type(^^number));
static_assert(is_base_value_type(^^boolean));
static_assert(is_base_value_type(^^array));
static_assert(is_base_value_type(^^object));

} // namespace detail

struct value : detail::base_value
{
  using detail::base_value::base_value;

  constexpr value() = default;

  // === numeric catch-all (int, float, size_t, …)
  constexpr value(detail::number_c auto n)
    requires(not std::constructible_from<detail::base_value, decltype(n)>)
      : detail::base_value(number(n))
  {}

  // === initializer-list construction  {"key": val, ...}
  constexpr value(std::initializer_list<std::pair<string const, value>> pairs)
      : detail::base_value(object(pairs))
  {}

  // === type predicates
  constexpr bool is_null() const
  {
    return std::holds_alternative<null_t>(*this);
  }
  constexpr bool is_string() const
  {
    return std::holds_alternative<string>(*this);
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

  // === typed accessors
  template <typename T> constexpr decltype(auto) as(this auto&& self)
  {
    return std::get<T>(self);
  }

  // === get<T>() with optional fallback
  template <typename T, typename Self>
  constexpr std::optional<const_like_t<Self, T>&> get(this Self& self) noexcept
  {
    if(auto* p = std::get_if<T>(&self))
      return *p;
    return std::nullopt;
  }

  // === array index access
  constexpr decltype(auto) operator[](this auto&& self, std::size_t idx)
  {
    return self.template as<array>()[idx];
  }

  // == object key access
  constexpr value& operator[](std::string_view key)
  {
    auto val = at_path(key);
    if(val == nullptr)
    {
      throw std::runtime_error("insert failed");
    }
    return *val;
  }

  constexpr std::optional<value const&> at(std::string_view key) const noexcept
  {
    auto val = at_path(key);
    if(val == nullptr)
    {
      return std::nullopt;
    }
    return *val;
  }

private:
  //    Returns a pointer to the nested value, or nullptr if any step is missing.
  constexpr auto at_path(this auto& self, std::string_view path) noexcept
      -> const_like_t<decltype(self), value>*
  {
    auto cur = &self;
    while(!path.empty())
    {
      if(!cur->is_object())
      {
        if constexpr(requires { *cur = object{}; })
        {
          *cur = object{};
        }
        else
        {
          return nullptr;
        }
      }
      const auto     dot = path.find('.');
      const auto     key = path.substr(0, dot);
      decltype(auto) obj = cur->template as<object>();
      auto it = std::ranges::find_if(obj, [key](auto const& elem) { return elem.first == key; });
      if(it == obj.end())
      {
        if constexpr(requires { obj.emplace(key, null); })
        {
          it = obj.emplace(key, null).first;
        }
        else
        {
          return nullptr;
        }
      }
      cur  = &it->second;
      path = (dot == std::string_view::npos) ? std::string_view{} : path.substr(dot + 1);
    }
    return cur;
  }

public:
  // === contains  (single key or dotted path)
  constexpr bool contains(std::string_view path) const noexcept
  {
    return at_path(path) != nullptr;
  }

  // === size / empty ===
  constexpr std::size_t size() const
  {
    return std::visit(
        match{
            [](null_t const&) -> std::size_t { return 0; },
            [](string const& s) -> std::size_t { return s.size(); },
            [](number) -> std::size_t { return 1; },
            [](boolean) -> std::size_t { return 1; },
            [](array const& a) -> std::size_t { return a.size(); },
            [](object const& o) -> std::size_t { return o.size(); },
        },
        static_cast<detail::base_value const&>(*this));
  }

  constexpr bool empty() const
  {
    return size() == 0;
  }

  // === push_back for arrays
  constexpr void push_back(value v)
  {
    as<array>().push_back(std::move(v));
  }

  // === merge another object into this one
  constexpr void merge(object const& other)
  {
    auto& obj = as<object>();
    for(auto const& [k, v] : other)
      obj.insert_or_assign(k, v);
  }

  // === visit helper
  constexpr decltype(auto) visit(this auto&& self, auto&& fn)
  {
    return reflex::visit(std::forward<decltype(fn)>(fn), std::forward<decltype(self)>(self));
  }

  // === equality operators
  constexpr bool operator==(null_t const&) const
  {
    return is_null();
  }

  constexpr bool operator==(detail::str_c auto const& s) const
  {
    return is_string() and as<string>() == s;
  }

  constexpr bool operator==(detail::number_c auto num) const
  {
    return is_number() and as<number>() == num;
  }

  constexpr bool operator==(boolean b) const
  {
    return is_boolean() and as<boolean>() == b;
  }

  constexpr bool operator==(detail::seq_c auto const& arr) const
  {
    return is_array() and as<array>() == arr;
  }

  constexpr bool operator==(detail::map_c auto const& obj) const
  {
    return is_object() and as<object>() == obj;
  }
};

} // namespace reflex::serde::json

export namespace reflex::serde
{
template <> struct object_visitor<json::object>
{
  template <typename Fn, typename StrT, decays_to_c<json::object> Obj>
  static inline constexpr decltype(auto)
      operator()(Fn&& fn, StrT&& key, Obj&& obj) // noexcept(noexcept(fwd(fn)(std::get<0>(fwd(v)))))
  {
    if(obj.contains(key))
    {
      return std::forward<Fn>(fn)(std::forward<Obj>(obj)[std::forward<StrT>(key)]);
    }
    else
    {
      return std::forward<Fn>(fn)(std::forward<Obj>(obj)
                                      .emplace(std::forward<decltype(key)>(key), json::null)
                                      .first->second);
    }
  }
};
} // namespace reflex::serde

export namespace std
{
template <> struct formatter<reflex::serde::json::null_t> : formatter<std::string_view>
{
  auto format(reflex::serde::json::null_t const&, auto& ctx) const
  {
    return std::formatter<std::string_view>::format("null", ctx);
  }
};

template <> struct formatter<reflex::serde::json::value> : formatter<std::string_view>
{
  auto format(reflex::serde::json::value const& v, auto& ctx) const
  {
    using namespace reflex;
    using namespace reflex::serde::json;
    using reflex::serde::json::array;
    v.visit(
        match{
            [&ctx](null_t const&) { std::format_to(ctx.out(), "null"); },
            [&ctx](string const& s) { std::format_to(ctx.out(), "{}", s); },
            [&ctx](number n) { std::format_to(ctx.out(), "{}", n); },
            [&ctx](boolean b) { std::format_to(ctx.out(), "{}", b ? "true" : "false"); },
            [&ctx](array const& arr) {
              ctx.out() = '[';
              for(std::size_t i = 0; i < arr.size(); ++i)
              {
                if(i > 0)
                  ctx.out() = ',';
                std::format_to(ctx.out(), "{}", arr[i]);
              }
              ctx.out() = ']';
            },
            [&ctx](object const& obj) {
              ctx.out()  = '{';
              bool first = true;
              for(auto const& [k, v] : obj)
              {
                if(!first)
                  ctx.out() = ',';
                first = false;
                std::format_to(ctx.out(), "\"{}\":{}", k, v);
              }
              ctx.out() = '}';
            },
        });
    return ctx.out();
  }
};

} // namespace std