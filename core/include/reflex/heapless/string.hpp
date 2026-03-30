#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <format>
#endif

#include <reflex/heapless/vector.hpp>

REFLEX_EXPORT namespace reflex::heapless
{
  template <typename CharT, std::size_t N> struct basic_string : vector<CharT, N>
  {
    using vector_type = vector<CharT, N>;

    using vector_type::vector_type;

    template <std::size_t M> constexpr basic_string(const CharT (&str)[M])
    {
      static_assert(M <= N);
      this->assign(str, str + M - 1); // exclude null terminator
    }

    constexpr basic_string(std::basic_string_view<CharT> sv)
    {
      if(sv.size() > N)
      {
        throw std::length_error("String literal too long for basic_string");
      }
      this->assign(sv.begin(), sv.end());
    }

    constexpr std::basic_string_view<CharT> view() const noexcept
    {
      return {this->data(), this->size()};
    }

    constexpr operator std::basic_string_view<CharT>() const noexcept
    {
      return view();
    }

    constexpr bool operator==(std::basic_string_view<CharT> other) const
    {
      return view() == other;
    }

    constexpr auto operator<=>(std::basic_string_view<CharT> other) const
    {
      return view() <=> other;
    }

    constexpr auto operator+=(std::basic_string_view<CharT> suffix)
    {
      if(this->size() + suffix.size() > N)
      {
        throw std::length_error("Resulting string too long for basic_string");
      }
      this->insert(this->end(), suffix.begin(), suffix.end());
      return *this;
    }

    constexpr auto operator+=(CharT c)
    {
      if(this->size() + 1 > N)
      {
        throw std::length_error("Resulting string too long for basic_string");
      }
      this->push_back(c);
      return *this;
    }
  };

  template <std::size_t N> using string = basic_string<char, N>;

} // namespace reflex::heapless

REFLEX_EXPORT namespace std
{
  template <typename CharT, std::size_t N>
  struct formatter<reflex::heapless::basic_string<CharT, N>>
      : formatter<std::basic_string_view<CharT>>
  {
    template <typename FormatContext>
    constexpr auto
        format(const reflex::heapless::basic_string<CharT, N>& str, FormatContext& ctx) const
    {
      return formatter<std::basic_string_view<CharT>>::format(str.view(), ctx);
    }
  };
} // namespace std
