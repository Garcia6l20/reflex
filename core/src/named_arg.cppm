export module reflex.core:named_arg;

import std;

export namespace reflex
{
template <typename T> struct named_arg
{
  std::string_view name;
  T                value;
};

struct arg_name
{
  std::string_view name;

  template <typename T> named_arg<T> operator=(T&& value) const
  {
    return {name, std::forward<T>(value)};
  }
};

consteval arg_name operator""_na(const char* data, std::size_t N)
{
  return arg_name{
      std::string_view{data, N}
  };
}
} // namespace reflex