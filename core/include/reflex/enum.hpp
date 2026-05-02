#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#include <reflex/concepts.hpp>
#include <reflex/meta.hpp>
#include <reflex/derive.hpp>
#include <reflex/parse.hpp>
#include <reflex/format.hpp>

REFLEX_EXPORT namespace reflex
{
  /// @brief An annotation that can be applied to enum types to indicate that they should be treated
  /// as bit flags.
  constexpr struct __enum_flags
  {
  } EnumFlags;

  template <typename E>
  constexpr bool enum_flags_v = enum_c<E> and derives(^^E, EnumFlags);

  template <typename E>
  concept enum_flags_c = enum_flags_v<std::remove_cvref_t<E>>;

  namespace bitwise_operations {

    template <enum_flags_c EF> constexpr auto operator|(EF lhs, EF rhs) noexcept -> EF
    {
      using underlying = std::underlying_type_t<EF>;
      return static_cast<EF>(static_cast<underlying>(lhs) | static_cast<underlying>(rhs));
    }

    template <enum_flags_c EF> constexpr auto operator&(EF lhs, EF rhs) noexcept -> EF
    {
      using underlying = std::underlying_type_t<EF>;
      return static_cast<EF>(static_cast<underlying>(lhs) & static_cast<underlying>(rhs));
    }

    template <enum_flags_c EF> constexpr auto operator^(EF lhs, EF rhs) noexcept -> EF
    {
      using underlying = std::underlying_type_t<EF>;
      return static_cast<EF>(static_cast<underlying>(lhs) ^ static_cast<underlying>(rhs));
    }

    template <enum_flags_c EF> constexpr auto operator~(EF value) noexcept -> EF
    {
      using underlying = std::underlying_type_t<EF>;
      return static_cast<EF>(~static_cast<underlying>(value));
    }

    template <enum_flags_c EF> constexpr auto operator|=(EF& lhs, EF rhs) noexcept -> EF&
    {
      lhs = lhs | rhs;
      return lhs;
    }

    template <enum_flags_c EF> constexpr auto operator&=(EF& lhs, EF rhs) noexcept -> EF&
    {
      lhs = lhs & rhs;
      return lhs;
    }

    template <enum_flags_c EF> constexpr auto operator^=(EF& lhs, EF rhs) noexcept -> EF&
    {
      lhs = lhs ^ rhs;
      return lhs;
    }
  } // namespace bitwise_operations

  using namespace bitwise_operations;

  template <enum_c E> constexpr std::string_view enum_value_name(E value)
  {
    template for(constexpr auto e : define_static_array(enumerators_of(^^E)))
    {
      if(value == [:e:])
      {
        return identifier_of(e);
      }
    }
    throw std::runtime_error("Invalid enum value");
  }

  template <enum_c E> constexpr std::optional<E> to_enum_value(std::string_view name)
  {
    template for(constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^E)))
    {
      if(name == std::meta::identifier_of(e))
      {
        return [:e:];
      }
    }

    return std::nullopt;
  }

  template <enum_c E>
    requires(not enum_flags_c<E> and derives(^^E, Parse))
  struct parser<E>
  {
    constexpr parse_result<E> operator()(std::string_view s) const noexcept
    {
      template for(constexpr auto e : define_static_array(enumerators_of(^^E)))
      {
        if(identifier_of(e) == s)
        {
          return extract<E>(e);
        }
      }
      return std::unexpected(std::make_error_code(std::errc::invalid_argument));
    }
  };

  template <enum_flags_c E>
    requires(derives(^^E, Parse))
  struct parser<E>
  {
    std::size_t prefix_len = 0;

    constexpr void parse(std::string_view spec)
    {
      if(!spec.empty() && spec[0] == '-')
      {
        auto [ptr, ec] = std::from_chars(spec.data() + 1, spec.data() + spec.size(), prefix_len);
        if(ec != std::errc())
        {
          throw std::format_error("Invalid format specifier for enum flags parser");
        }
      }
    }

    constexpr parse_result<E> operator()(std::string_view s) const noexcept
    {
      E result{};

      while(!s.empty())
      {
        auto token = s.substr(0, s.find('|'));
        s.remove_prefix(std::min(s.size(), token.size() + 1));

        bool found = false;
        template for(constexpr auto e : define_static_array(enumerators_of(^^E)))
        {
          if(identifier_of(e).substr(prefix_len) == token)
          {
            result |= [:e:];
            found = true;
            break;
          }
        }
        if(!found)
        {
          return std::unexpected(std::make_error_code(std::errc::invalid_argument));
        }
      }
      return result;
    }
  };

} // namespace reflex

REFLEX_EXPORT namespace std
{
  template <reflex::enum_flags_c E, typename CharT>
    requires(reflex::derives(^^E, reflex::Format))
  struct formatter<E, CharT>
  {
    std::size_t prefix_len = 0;

    constexpr auto parse(auto& ctx)
    {
      // Check for optional '-<N>' flag for prefix removal
      auto it = ctx.begin();
      const auto end = ctx.end();
      if(it != end && *it == '-')
      {
        ++it;
        auto [ptr, ec] = std::from_chars(&*it, &*end, prefix_len);
        if(ec != std::errc())
        {
          throw std::format_error("Invalid format specifier for enum flags");
        }
        it = ptr;
      }
      return it;
    }

    constexpr auto format(const E& value, auto& ctx) const
    {
      using reflex::bitwise_operations::operator&;

      using underlying = std::underlying_type_t<E>;
      using unsigned_underlying = std::make_unsigned_t<underlying>;

      std::string result;

      template for(constexpr auto e : define_static_array(enumerators_of(^^E)))
      {
        if constexpr (has_single_bit(unsigned_underlying([:e:])))
        {
          if((value & [:e:]) == [:e:])
          {
            if(!result.empty())
            {
              result += '|';
            }
            result += identifier_of(e).substr(prefix_len);
          }
        }
      }

      return std::format_to(ctx.out(), "{}", result);
    }
  };

  template <reflex::enum_c E, typename CharT>
    requires(not reflex::enum_flags_c<E> and reflex::derives(^^E, reflex::Format))
  struct formatter<E, CharT>
  {
    constexpr auto parse(auto& ctx)
    {
      return ctx.begin();
    }

    constexpr auto format(const E& e, auto& ctx) const
    {
      return format_to(ctx.out(), "{}", reflex::enum_value_name(e));
    }
  };
} // namespace std
