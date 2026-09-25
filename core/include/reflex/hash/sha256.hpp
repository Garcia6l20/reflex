#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <array>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <span>
#include <string_view>
#endif

REFLEX_EXPORT namespace reflex
{
  namespace _sha256_detail
  {
  inline constexpr std::array<std::uint32_t, 64> k{
      0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u,
      0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
      0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u,
      0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
      0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
      0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
      0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
      0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
      0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au,
      0x5b9cca4fu, 0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
      0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};

  template <typename T>
  concept byte_like_c = std::same_as<T, char> or std::same_as<T, unsigned char>
                     or std::same_as<T, std::byte> or std::same_as<T, char8_t>;

  template <typename R>
  concept byte_range_c = std::ranges::contiguous_range<R const> and std::ranges::sized_range<R const>
                      and byte_like_c<std::ranges::range_value_t<R const>>;
  }

  /** @brief Incremental FIPS 180-4 SHA-256 hasher.
   *
   * Fully `constexpr`: absorbing data and finalizing both work in a constant
   * expression, in addition to the usual runtime use.
   */
  class sha256
  {
  public:
    /** @brief Number of bytes in a SHA-256 digest. */
    static constexpr std::size_t digest_size = 32;
    /** @brief 32-byte SHA-256 digest. */
    using digest = std::array<std::byte, digest_size>;

    /** @brief Constructs a hasher holding the initial FIPS 180-4 state. */
    constexpr sha256() noexcept
    {
      reset();
    }

    /** @brief Restores the initial FIPS 180-4 state, discarding any absorbed data. */
    constexpr void reset() noexcept
    {
      state_      = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                      0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};
      buffer_len_ = 0;
      bit_len_    = 0;
    }

    /** @brief Absorbs @p data into the running hash.
     * @param data the bytes to absorb, appended after any data already absorbed.
     */
    constexpr void update(std::span<std::byte const> data) noexcept
    {
      bit_len_ += static_cast<std::uint64_t>(data.size()) * 8;
      absorb(data.data(), data.size());
    }

    /** @brief Absorbs the bytes of @p s into the running hash.
     * @param s the characters to absorb, appended after any data already absorbed.
     */
    constexpr void update(std::string_view s) noexcept
    {
      bit_len_ += static_cast<std::uint64_t>(s.size()) * 8;
      absorb(s.data(), s.size());
    }

    /** @brief Absorbs a contiguous, sized range of `char`, `char8_t`, `unsigned char` or `std::byte`.
     * @param r the range to absorb, appended after any data already absorbed.
     */
    template <_sha256_detail::byte_range_c R>
      requires(not std::convertible_to<R const&, std::string_view>)
    constexpr void update(R const& r) noexcept
    {
      bit_len_ += static_cast<std::uint64_t>(std::ranges::size(r)) * 8;
      absorb(std::ranges::data(r), std::ranges::size(r));
    }

    /** @brief Pads and finalizes a copy of the state, leaving @c *this untouched.
     *
     * Safe to call repeatedly, each call returns the same digest. @c update() may
     * be called again afterwards to continue absorbing more data from where
     * @c finish() left off.
     *
     * @return the 32-byte digest of everything absorbed so far.
     */
    constexpr digest finish() const noexcept
    {
      sha256 copy = *this;

      std::array<std::byte, 64> pad{};
      pad[0]              = std::byte{0x80};
      std::size_t pad_len = copy.buffer_len_ < 56 ? 56 - copy.buffer_len_ : 120 - copy.buffer_len_;
      copy.absorb(pad.data(), pad_len);

      std::array<std::byte, 8> length_be{};
      for(std::size_t i = 0; i < 8; ++i)
      {
        length_be[i] = static_cast<std::byte>(copy.bit_len_ >> (56 - 8 * i));
      }
      copy.absorb(length_be.data(), length_be.size());

      digest out{};
      for(std::size_t i = 0; i < 8; ++i)
      {
        std::uint32_t w = copy.state_[i];
        out[4 * i + 0]  = static_cast<std::byte>(w >> 24);
        out[4 * i + 1]  = static_cast<std::byte>(w >> 16);
        out[4 * i + 2]  = static_cast<std::byte>(w >> 8);
        out[4 * i + 3]  = static_cast<std::byte>(w);
      }
      return out;
    }

  private:
    template <_sha256_detail::byte_like_c T>
    constexpr void absorb(T const* data, std::size_t total) noexcept
    {
      std::size_t offset = 0;

      if(buffer_len_ != 0)
      {
        std::size_t room = buffer_.size() - buffer_len_;
        std::size_t take = (total < room) ? total : room;
        for(std::size_t i = 0; i < take; ++i)
        {
          buffer_[buffer_len_ + i] = static_cast<std::byte>(static_cast<unsigned char>(data[i]));
        }
        buffer_len_ += take;
        offset += take;

        if(buffer_len_ < buffer_.size())
        {
          return;
        }

        process_block(buffer_.data());
        buffer_len_ = 0;
      }

      while(total - offset >= buffer_.size())
      {
        process_block(data + offset);
        offset += buffer_.size();
      }

      std::size_t remaining = total - offset;
      for(std::size_t i = 0; i < remaining; ++i)
      {
        buffer_[i] = static_cast<std::byte>(static_cast<unsigned char>(data[offset + i]));
      }
      buffer_len_ = remaining;
    }

    template <_sha256_detail::byte_like_c T>
    [[gnu::noinline]] constexpr void process_block(T const* block) noexcept
    {
      std::array<std::uint32_t, 64> w{};
      for(std::size_t i = 0; i < 16; ++i)
      {
        w[i] = (static_cast<std::uint32_t>(static_cast<unsigned char>(block[4 * i])) << 24)
             | (static_cast<std::uint32_t>(static_cast<unsigned char>(block[4 * i + 1])) << 16)
             | (static_cast<std::uint32_t>(static_cast<unsigned char>(block[4 * i + 2])) << 8)
             | static_cast<std::uint32_t>(static_cast<unsigned char>(block[4 * i + 3]));
      }
      for(std::size_t i = 16; i < 64; ++i)
      {
        std::uint32_t s0 = std::rotr(w[i - 15], 7) ^ std::rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        std::uint32_t s1 = std::rotr(w[i - 2], 17) ^ std::rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i]              = w[i - 16] + s0 + w[i - 7] + s1;
      }

      std::uint32_t a = state_[0];
      std::uint32_t b = state_[1];
      std::uint32_t c = state_[2];
      std::uint32_t d = state_[3];
      std::uint32_t e = state_[4];
      std::uint32_t f = state_[5];
      std::uint32_t g = state_[6];
      std::uint32_t h = state_[7];

      for(std::size_t i = 0; i < 64; ++i)
      {
        std::uint32_t s1    = std::rotr(e, 6) ^ std::rotr(e, 11) ^ std::rotr(e, 25);
        std::uint32_t ch    = (e & f) ^ (~e & g);
        std::uint32_t temp1 = h + s1 + ch + _sha256_detail::k[i] + w[i];
        std::uint32_t s0    = std::rotr(a, 2) ^ std::rotr(a, 13) ^ std::rotr(a, 22);
        std::uint32_t maj   = (a & b) ^ (a & c) ^ (b & c);
        std::uint32_t temp2 = s0 + maj;

        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
      }

      state_[0] += a;
      state_[1] += b;
      state_[2] += c;
      state_[3] += d;
      state_[4] += e;
      state_[5] += f;
      state_[6] += g;
      state_[7] += h;
    }

    std::array<std::uint32_t, 8> state_{};
    std::array<std::byte, 64>    buffer_{};
    std::size_t                  buffer_len_ = 0;
    std::uint64_t                bit_len_    = 0;
  };

  /** @brief One-shot SHA-256 digest of @p data.
   * @param data the bytes to hash.
   * @return the 32-byte digest of @p data.
   */
  constexpr sha256::digest sha256_of(std::span<std::byte const> data) noexcept
  {
    sha256 h;
    h.update(data);
    return h.finish();
  }

  /** @brief One-shot SHA-256 digest of @p s.
   * @param s the characters to hash.
   * @return the 32-byte digest of @p s.
   */
  constexpr sha256::digest sha256_of(std::string_view s) noexcept
  {
    sha256 h;
    h.update(s);
    return h.finish();
  }

  /** @brief One-shot SHA-256 digest of a contiguous, sized range of `char`, `char8_t`, `unsigned char` or `std::byte`.
   * @param r the range to hash.
   * @return the 32-byte digest of @p r.
   */
  template <_sha256_detail::byte_range_c R>
    requires(not std::convertible_to<R const&, std::string_view>)
  constexpr sha256::digest sha256_of(R const& r) noexcept
  {
    sha256 h;
    h.update(r);
    return h.finish();
  }

  /** @brief Lower-case hex encoding of @p d, as a fixed 64-character buffer.
   *
   * The result is not NUL-terminated.
   *
   * @param d the digest to encode.
   * @return 64 lower-case hex digits, two per byte of @p d.
   */
  constexpr std::array<char, sha256::digest_size * 2> to_hex(sha256::digest const& d) noexcept
  {
    constexpr char digits[] = "0123456789abcdef";
    std::array<char, sha256::digest_size * 2> out{};
    for(std::size_t i = 0; i < d.size(); ++i)
    {
      auto v         = std::to_integer<unsigned>(d[i]);
      out[2 * i]     = digits[v >> 4];
      out[2 * i + 1] = digits[v & 0xF];
    }
    return out;
  }
}
