#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <concepts>
#include <format>
#include <type_traits>
#endif

#include <reflex/heapless/vector.hpp>

REFLEX_EXPORT namespace reflex::heapless
{
  template <typename CharT, std::size_t N> struct basic_string : vector<CharT, N>
  {
    using view_type            = std::basic_string_view<CharT>;
    static constexpr auto npos = view_type::npos;

    using vector_type = vector<CharT, N>;

    using vector_type::vector_type;

    template <std::size_t M> constexpr basic_string(const CharT (&str)[M])
    {
      static_assert(M <= N);
      this->assign(str, str + M - 1); // exclude null terminator
    }

    constexpr basic_string(view_type sv)
    {
      if(sv.size() > N)
      {
        throw std::length_error("String literal too long for basic_string");
      }
      this->assign(sv.begin(), sv.end());
    }

    template <typename StringLike>
      requires(
          not std::same_as<std::remove_cvref_t<StringLike>, basic_string>
          and std::convertible_to<StringLike const&, view_type>)
    constexpr basic_string(StringLike const& str) : basic_string(view_type{str})
    {}

    constexpr view_type view() const noexcept
    {
      return {this->data(), this->size()};
    }

    constexpr operator view_type() const noexcept
    {
      return view();
    }

    friend constexpr bool operator==(basic_string const& lhs, basic_string const& rhs)
    {
      return lhs.view() == rhs.view();
    }

    friend constexpr bool operator==(basic_string const& lhs, view_type rhs)
    {
      return lhs.view() == rhs;
    }

    friend constexpr bool operator==(view_type lhs, basic_string const& rhs)
    {
      return lhs == rhs.view();
    }

    friend constexpr auto operator<=>(basic_string const& lhs, basic_string const& rhs)
    {
      return lhs.view() <=> rhs.view();
    }

    friend constexpr auto operator<=>(basic_string const& lhs, view_type rhs)
    {
      return lhs.view() <=> rhs;
    }

    friend constexpr auto operator<=>(view_type lhs, basic_string const& rhs)
    {
      return lhs <=> rhs.view();
    }

    constexpr auto operator+=(view_type suffix)
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

    using vector_type::operator=;

    constexpr basic_string& operator=(std::basic_string_view<CharT> const& other)
    {
      const auto other_size = std::ranges::size(other);
      if(other_size > N)
      {
        throw std::length_error("String assignment too long for basic_string");
      }

      this->clear();
      this->insert(this->end(), std::ranges::begin(other), std::ranges::end(other));
      return *this;
    }

    template <typename StringLike>
      requires(
          not std::same_as<std::remove_cvref_t<StringLike>, basic_string>
          and std::convertible_to<StringLike const&, view_type>)
    constexpr basic_string& operator=(StringLike const& other)
    {
      return (*this = view_type{other});
    }

    constexpr auto erase(std::size_t pos, std::size_t count = 1)
    {
      if(pos >= this->size())
      {
        return;
      }
      count = std::min(count, this->size() - pos);
      vector_type::erase(this->begin() + pos, this->begin() + pos + count);
    }

    constexpr auto replace(std::size_t pos, std::size_t count, view_type replacement)
    {
      if(pos > this->size())
      {
        return;
      }
      count = std::min(count, this->size() - pos);
      if(this->size() - count + replacement.size() > N)
      {
        throw std::length_error("Resulting string too long for basic_string");
      }
      vector_type::erase(this->begin() + pos, this->begin() + pos + count);
      this->insert(this->begin() + pos, replacement.begin(), replacement.end());
    }

    constexpr auto substr(std::size_t pos, std::size_t count = basic_string::npos) const
    {
      if(pos > this->size())
      {
        return view_type{};
      }
      count = std::min(count, this->size() - pos);
      return view_type{this->data() + pos, count};
    }

    template <typename Prefix> constexpr bool starts_with(Prefix prefix) const noexcept
    {
      return view().starts_with(prefix);
    }

    template <typename Suffix> constexpr bool ends_with(Suffix suffix) const noexcept
    {
      return view().ends_with(suffix);
    }

    friend std::basic_ostream<CharT>&
        operator<<(std::basic_ostream<CharT>& os, basic_string const& str)
    {
      return os << str.view();
    }
  };

  template <std::size_t N> using string = basic_string<char, N>;

} // namespace reflex::heapless

REFLEX_EXPORT namespace std
{
  template <typename CharT, std::size_t N, typename Traits>
  struct common_type<reflex::heapless::basic_string<CharT, N>, std::basic_string_view<CharT, Traits>>
  {
    using type = std::basic_string_view<CharT, Traits>;
  };

  template <typename CharT, std::size_t N, typename Traits>
  struct common_type<std::basic_string_view<CharT, Traits>, reflex::heapless::basic_string<CharT, N>>
  {
    using type = std::basic_string_view<CharT, Traits>;
  };

  template <typename CharT, std::size_t N, typename Traits, template <typename> typename TQual,
            template <typename> typename UQual>
  struct basic_common_reference<
      reflex::heapless::basic_string<CharT, N>, std::basic_string_view<CharT, Traits>, TQual,
      UQual>
  {
    using type = std::basic_string_view<CharT, Traits>;
  };

  template <typename CharT, std::size_t N, typename Traits, template <typename> typename TQual,
            template <typename> typename UQual>
  struct basic_common_reference<
      std::basic_string_view<CharT, Traits>, reflex::heapless::basic_string<CharT, N>, TQual,
      UQual>
  {
    using type = std::basic_string_view<CharT, Traits>;
  };

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
