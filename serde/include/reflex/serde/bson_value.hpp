#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <reflex/serde/poly.hpp>
#endif

REFLEX_EXPORT namespace reflex::serde::bson
{
  using null_t = reflex::poly::null_t;

  constexpr null_t null{};

  using int32 = std::int32_t;
  using int64 = std::int64_t;

  using string     = std::string;
  using number     = double;
  using boolean    = bool;
  using decimal128 = std::float128_t;

  struct datetime
  {
    using rep = std::int64_t;
    rep millis_since_epoch{};

    constexpr datetime() = default;
    constexpr datetime(rep millis) noexcept : millis_since_epoch(millis)
    {}
    constexpr datetime(std::chrono::system_clock::time_point tp) noexcept
        : millis_since_epoch(
              std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count())
    {}

    static constexpr datetime now() noexcept
    {
      return datetime(std::chrono::system_clock::now());
    }

    template <template <typename, typename> typename ClockT, typename Rep, typename Period>
      requires(std::chrono::is_clock_v<ClockT<Rep, Period>>)
    operator std::chrono::time_point<ClockT<Rep, Period>, std::chrono::duration<Rep, Period>>()
        const noexcept
    {
      return {std::chrono::milliseconds(millis_since_epoch)};
    }

    constexpr operator rep() const noexcept
    {
      return millis_since_epoch;
    }

    constexpr bool operator==(datetime const&) const noexcept  = default;
    constexpr auto operator<=>(datetime const&) const noexcept = default;
  };

  using value  = poly::var<int32, int64, number, boolean, string, decimal128, datetime>;
  using object = value::obj_type;
  using array  = value::arr_type;

  constexpr parse_result<datetime> tag_invoke(
      tag_t<Parse>, std::string_view s, std::type_identity<datetime>) noexcept
  {
    std::uint64_t ms = 0;
    auto [ptr, ec]   = std::from_chars(s.data(), s.data() + s.size(), ms);
    if(ec != std::errc())
    {
      return std::unexpected(std::errc::invalid_argument);
    }
    return {datetime(ms), ptr};
  }
} // namespace reflex::serde::bson

REFLEX_EXPORT namespace std
{
  template <typename CharT>
  struct formatter<reflex::serde::bson::datetime, CharT>
      : formatter<std::basic_string_view<CharT>, CharT>
  {
    template <typename FormatContext>
    auto format(reflex::serde::bson::datetime const& dt, FormatContext& ctx) const
    {
      return format_to(ctx.out(), "{}", dt.millis_since_epoch);
    }
  };
}
