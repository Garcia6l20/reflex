#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <reflex/parse.hpp>
#include <reflex/serde/poly.hpp>

#include <array>
#include <chrono>
#include <cstdint>
#include <stdfloat>
#include <string>
#include <vector>
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

  /** @brief BSON binary data, element type 0x05, with its subtype.
   *
   * Any subtype reads and writes. For the deprecated subtype 0x02 `data` holds the payload alone:
   * reading checks the inner int32 length equals the payload size and drops it, writing puts it
   * back. JSON, YAML and TOML write it as the Extended JSON v2 `$binary` document.
   */
  struct binary
  {
    std::byte              subtype{0x00};
    std::vector<std::byte> data;

    constexpr bool operator==(binary const&) const = default;
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

  /** @brief A BSON ObjectId, element type 0x07, its 12 bytes in wire order. */
  struct object_id
  {
    std::array<std::byte, 12> bytes{};

    constexpr bool operator==(object_id const&) const = default;
  };

  /** @brief A BSON timestamp, element type 0x11, written increment first, both little-endian. */
  struct timestamp
  {
    std::uint32_t increment{};
    std::uint32_t seconds{};

    constexpr bool operator==(timestamp const&) const = default;
  };

  /** @brief A BSON regular expression, element type 0x0B. Neither string may hold a null byte. */
  struct regex
  {
    std::string pattern;
    std::string options;

    constexpr bool operator==(regex const&) const = default;
  };

  /** @brief BSON JavaScript code, element type 0x0D. */
  struct javascript
  {
    std::string code;

    constexpr bool operator==(javascript const&) const = default;
  };

  /** @brief A BSON symbol, element type 0x0E, deprecated by the spec. */
  struct symbol
  {
    std::string name;

    constexpr bool operator==(symbol const&) const = default;
  };

  /** @brief A BSON DBPointer, element type 0x0C, deprecated by the spec. */
  struct db_pointer
  {
    std::string ref;
    object_id   id;

    constexpr bool operator==(db_pointer const&) const = default;
  };

  /** @brief The BSON MinKey, element type 0xFF. */
  struct min_key
  {
    constexpr bool operator==(min_key const&) const = default;
  };

  /** @brief The BSON MaxKey, element type 0x7F. */
  struct max_key
  {
    constexpr bool operator==(max_key const&) const = default;
  };

  /** @brief The BSON undefined value, element type 0x06, deprecated by the spec. */
  struct undefined
  {
    constexpr bool operator==(undefined const&) const = default;
  };

  /** @brief BSON JavaScript code with scope, element type 0x0F, deprecated by the spec.
   *
   * @p scope holds the encoded scope document, length prefix and terminator included, so it
   * round-trips byte-exact, and defaults to the empty document. Reading checks its length and
   * terminator, not its elements. Writing throws std::invalid_argument when @p scope is not at
   * least an empty document whose length prefix matches its size, an explicitly empty vector
   * included.
   */
  struct code_w_scope
  {
    std::string            code;
    std::vector<std::byte> scope{
        std::byte{5}, std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0}};

    constexpr bool operator==(code_w_scope const&) const = default;
  };

  using value = poly::var<
      int32, int64, number, boolean, string, decimal128, datetime, binary, object_id, timestamp,
      regex, javascript, symbol, db_pointer, min_key, max_key, undefined, code_w_scope>;
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
