#include <doctest/doctest.h>

import reflex.core;
import std;

using namespace reflex;
using namespace std::literals;

namespace
{
constexpr sha256::digest to_digest(std::string_view hex)
{
  sha256::digest d{};
  for(std::size_t i = 0; i < d.size(); ++i)
  {
    auto nibble = [](char c) -> unsigned {
      return c <= '9' ? static_cast<unsigned>(c - '0') : static_cast<unsigned>(c - 'a' + 10);
    };
    d[i] = static_cast<std::byte>((nibble(hex[2 * i]) << 4) | nibble(hex[2 * i + 1]));
  }
  return d;
}

constexpr std::string_view hex_view(std::array<char, sha256::digest_size * 2> const& hex)
{
  return std::string_view{hex.data(), hex.size()};
}
}

TEST_CASE("reflex::sha256: FIPS 180-4 empty message")
{
  CHECK(sha256_of(""sv) == to_digest("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"sv));
  CHECK(hex_view(to_hex(sha256_of(""sv))) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"sv);
}

TEST_CASE("reflex::sha256: FIPS 180-4 abc")
{
  CHECK(sha256_of("abc"sv) == to_digest("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"sv));
}

TEST_CASE("reflex::sha256: FIPS 180-4 448-bit message")
{
  constexpr std::string_view msg = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
  CHECK(sha256_of(msg) == to_digest("248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"sv));
}

TEST_CASE("reflex::sha256: FIPS 180-4 896-bit two-block message")
{
  constexpr std::string_view msg =
      "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu";
  REQUIRE(msg.size() * 8 == 896);
  CHECK(sha256_of(msg) == to_digest("cf5b16a778af8380036ce59e7b0492370b249b11e8f07a51afac45037afee9d1"sv));
}

TEST_CASE("reflex::sha256: one million 'a'")
{
  std::string msg(1'000'000, 'a');
  CHECK(sha256_of(std::string_view{msg})
        == to_digest("cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"sv));
}

TEST_CASE("reflex::sha256: block-boundary lengths of 'a'")
{
  struct boundary_vector
  {
    std::size_t      length;
    std::string_view hex;
  };

  const boundary_vector vectors[] = {
      {55, "9f4390f8d30c2dd92ec9f095b65e2b9ae9b0a925a5258e241c9f1e910f734318"sv},
      {56, "b35439a4ac6f0948b6d6f9e3c6af0f5f590ce20f1bde7090ef7970686ec6738a"sv},
      {63, "7d3e74a05d7db15bce4ad9ec0658ea98e3f06eeecf16b4c6fff2da457ddc2f34"sv},
      {64, "ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb"sv},
      {65, "635361c48bb9eab14198e76ea8ab7f1a41685d6ad62aa9146d301d4f17eb0ae0"sv},
      {119, "31eba51c313a5c08226adf18d4a359cfdfd8d2e816b13f4af952f7ea6584dcfb"sv},
      {120, "2f3d335432c70b580af0e8e1b3674a7c020d683aa5f73aaaedfdc55af904c21c"sv},
  };

  for(auto const& v : vectors)
  {
    CAPTURE(v.length);
    std::string msg(v.length, 'a');
    CHECK(hex_view(to_hex(sha256_of(std::string_view{msg}))) == v.hex);
  }
}

TEST_CASE("reflex::sha256: finish() is idempotent and update() may continue after it")
{
  sha256 h;
  h.update("hello "sv);
  auto first  = h.finish();
  auto second = h.finish();
  CHECK(first == second);

  h.update("world"sv);
  auto combined = h.finish();
  CHECK(combined == sha256_of("hello world"sv));
}

TEST_CASE("reflex::sha256: char8_t range is accepted")
{
  std::u8string_view s = u8"reflex sha256";
  CHECK(sha256_of(s) == sha256_of("reflex sha256"sv));
}

TEST_CASE("reflex::sha256: split across many update() calls equals one-shot")
{
  std::string msg(5000, '\0');
  for(std::size_t i = 0; i < msg.size(); ++i)
  {
    msg[i] = static_cast<char>('a' + (i % 26));
  }

  auto const one_shot = sha256_of(std::string_view{msg});

  for(std::size_t chunk : {1, 3, 7, 55, 63, 64, 65, 127, 511, 4096})
  {
    sha256 h;
    std::string_view remaining{msg};
    while(not remaining.empty())
    {
      auto take = std::min(chunk, remaining.size());
      h.update(remaining.substr(0, take));
      remaining.remove_prefix(take);
    }
    CAPTURE(chunk);
    CHECK(h.finish() == one_shot);
  }
}

TEST_CASE("reflex::sha256: reset() restores the initial state")
{
  sha256 h;
  h.update("some data"sv);
  h.reset();
  CHECK(h.finish() == sha256_of(""sv));
}

TEST_CASE("reflex::sha256: byte-range and span overloads agree with string_view")
{
  std::string_view s = "reflex sha256"sv;
  std::vector<unsigned char> uchars(s.begin(), s.end());
  std::vector<std::byte> bytes;
  for(char c : s)
  {
    bytes.push_back(static_cast<std::byte>(c));
  }

  auto expected = sha256_of(s);
  CHECK(sha256_of(uchars) == expected);
  CHECK(sha256_of(bytes) == expected);
  CHECK(sha256_of(std::span<std::byte const>{bytes}) == expected);
}

TEST_CASE("reflex::sha256: constant evaluation")
{
  static_assert(sha256_of("abc"sv)
                == to_digest("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"sv));
  static_assert(hex_view(to_hex(sha256_of("abc"sv)))
                == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"sv);
}

struct hashlib_vector
{
  std::vector<unsigned char> data;
  std::string_view           hex;
};

TEST_CASE("reflex::sha256: cross-checked against Python hashlib (seed 1234)")
{
  const hashlib_vector vectors[] = {
      {{0x3b, 0x03, 0x2e, 0x11, 0x2a, 0x32, 0xb5, 0x79, 0x08, 0x0f, 0x08, 0xb1, 0xf7, 0xed, 0x4c,
        0x2e, 0x5d, 0x3a, 0x07, 0xf9, 0x7f, 0x21, 0xee, 0x23, 0x2d, 0x17, 0x8a, 0x20},
       "247b267a7b4254973d20ae116f8163dad747fe4b2f3b799ef2cf46da21930000"sv},
      {{0xf6, 0xb5, 0x88, 0x7f, 0x66, 0xe8, 0x09, 0x24, 0x02, 0xaa, 0x49, 0xf2, 0xc1, 0x55, 0x1b,
        0x27, 0xfe, 0x53, 0x26},
       "14ea4a22da56564c96bbc6e25b9819ca7e39507a1de8a6bff0b6e5b3c099b32a"sv},
      {{0x6e, 0x49, 0x0d, 0xb1, 0x38, 0x48, 0x9c, 0xe8, 0x14, 0xd5, 0x8d, 0x14, 0x5a, 0x8b, 0x4f,
        0x99, 0x4f, 0xed, 0x15, 0xc5, 0xb2, 0xfd, 0xae, 0xef, 0xf3, 0x17, 0xf1, 0x57, 0xe1, 0xe0,
        0x97, 0x8c, 0x3f, 0x5f, 0xd5},
       "8e746e80ce93858c9e76d7dd4ed3a7cef6abbc45df47859fe5fc3716a5da447a"sv},
      {{0x3d, 0x34, 0xf8, 0xc0, 0x82, 0x62, 0xb0, 0x37, 0x50, 0x89, 0x4f, 0xa5, 0xe4, 0x24, 0x28,
        0xca, 0x6d, 0x18, 0x92, 0x13, 0x70, 0x2c, 0xa2, 0x9c, 0xeb, 0x21, 0x83},
       "ee5766343b81ac7e09318a83acd3514737c9ffb5a6058ca14427e0be1afc69cd"sv},
      {{0xda, 0x67, 0x33, 0xcb}, "99606bf4018889593f650161c0d85227ff1023f2b226eda8d36e6fbc2c4e3b93"sv},
      {{0xeb, 0x78, 0xb8, 0x69, 0xd7, 0x59, 0x68, 0x9a, 0x1e, 0xb4, 0x4e, 0xff},
       "fc39b839dc79113c13006f1763fa77f22ba1bb927ca531f7612207333a8cd6fc"sv},
  };

  for(auto const& v : vectors)
  {
    CAPTURE(v.hex);
    CHECK(hex_view(to_hex(sha256_of(v.data))) == v.hex);
  }
}
