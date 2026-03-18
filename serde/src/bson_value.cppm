export module reflex.serde.bson:value;

import reflex.serde;
import reflex.poly;

import std;

export namespace reflex::serde::bson
{
using null_t = reflex::poly::null_t;

constexpr null_t null{};

using int32 = std::int32_t;
using int64 = std::int64_t;

using string  = std::string;
using number  = double;
using boolean = bool;

struct decimal128
{
  std::array<std::byte, 16> bytes{};

  constexpr bool operator==(decimal128 const&) const = default;
};

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

} // namespace reflex::serde::bson
