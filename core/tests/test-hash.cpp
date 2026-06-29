#include <doctest/doctest.h>

import reflex.core;
import std;

using namespace reflex;
using namespace reflex::literals;
using namespace std::literals;

// hash must be deterministic: equal inputs hash equal, distinct inputs differ.
// No byte-for-byte parity with std::hash is required.

TEST_CASE("reflex::hash: integral identity is deterministic")
{
  CHECK(hash(false) == hash(false));
  CHECK(hash(true) == hash(true));
  CHECK(hash(false) != hash(true));

  CHECK(hash('A') == hash('A'));
  CHECK(hash(static_cast<signed char>(-1)) == hash(static_cast<signed char>(-1)));
  CHECK(hash(static_cast<unsigned char>(255)) == hash(static_cast<unsigned char>(255)));

  CHECK(hash(L'Z') == hash(L'Z'));
  CHECK(hash(u8'x') == hash(u8'x'));
  CHECK(hash(u'€') == hash(u'€'));
  CHECK(hash(U'😀') == hash(U'😀'));

  CHECK(hash(static_cast<short>(-32768)) == hash(static_cast<short>(-32768)));
  CHECK(hash(0) == 0u);
  CHECK(hash(-1) == hash(-1));
  CHECK(hash(std::numeric_limits<int>::min()) == hash(std::numeric_limits<int>::min()));
  CHECK(hash(std::numeric_limits<int>::max()) == hash(std::numeric_limits<int>::max()));
  CHECK(hash(std::numeric_limits<long long>::max())
        == hash(std::numeric_limits<long long>::max()));
  CHECK(hash(std::numeric_limits<unsigned long long>::max())
        == hash(std::numeric_limits<unsigned long long>::max()));
}

enum class Color : int
{
  Red = -3,
  Green = 0,
  Blue = 7
};

TEST_CASE("reflex::hash: enum hashes underlying value")
{
  CHECK(hash(Color::Red) == hash(Color::Red));
  CHECK(hash(Color::Green) == hash(Color::Green));
  CHECK(hash(Color::Blue) == hash(Color::Blue));
  CHECK(hash(Color::Blue) == static_cast<std::size_t>(7));
}

TEST_CASE("reflex::hash: strings are deterministic, all length classes")
{
  // Lengths 0..17 exercise every Murmur tail branch (len & 7 == 0..7 on 64-bit,
  // len % 4 on 32-bit) plus multiple full main-loop iterations.
  std::string s;
  for(std::size_t n = 0; n <= 17; ++n)
  {
    std::string_view sv{s};
    CHECK(hash(sv) == hash(sv));
    CHECK(hash(sv) == hash(s));
    CHECK(hash(s.c_str()) == hash(s));
    s.push_back(static_cast<char>('a' + (n % 26)));
  }
}

TEST_CASE("reflex::hash: high bytes promote as unsigned char")
{
  std::string s;
  s.push_back(static_cast<char>(0x80));
  s.push_back(static_cast<char>(0xFF));
  s.push_back(static_cast<char>(0x01));
  s.push_back(static_cast<char>(0xAB));
  s.push_back(static_cast<char>(0xCD));
  CHECK(hash(s) == hash(std::string_view{s}));
}

TEST_CASE("reflex::hash: empty string")
{
  CHECK(hash(std::string_view{}) == hash(""sv));
}

TEST_CASE("reflex::hash: constant_string input")
{
  static constexpr auto cs = "hello world"_sc;
  CHECK(hash(cs) == hash("hello world"sv));
}

// Compile-time path: the CPO is usable in constant expressions.
TEST_CASE("reflex::hash: constant evaluation")
{
  static_assert(hash(0) == 0u);
  static_assert(hash(42) == static_cast<std::size_t>(42));
  static_assert(hash('A') == static_cast<std::size_t>('A'));
  static_assert(hash(Color::Blue) == static_cast<std::size_t>(7));

  static_assert(hash("reflex") == hash("reflex"sv));
  static_assert(hash_bytes("", 0) == hash(""sv));
}

// Negative: unsupported types are not hashable (no tag_invoke overload).
TEST_CASE("reflex::hash: rejects unsupported types")
{
  static_assert(not hashable_c<int*>);  // pointer
  static_assert(not hashable_c<double>); // floating point
  static_assert(not hashable_c<float>);
  static_assert(hashable_c<int>);
  static_assert(hashable_c<std::string_view>);
}

// Extensibility: a user type opts in via tag_invoke.
namespace user
{
struct point
{
  int x, y;
};
constexpr std::size_t tag_invoke(tag_t<reflex::hash>, point const& p) noexcept
{
  return reflex::hash(p.x) * 31 + reflex::hash(p.y);
}
} // namespace user

TEST_CASE("reflex::hash: extensible via tag_invoke")
{
  static_assert(hashable_c<user::point>);
  static_assert(hash(user::point{1, 2}) == static_cast<std::size_t>(1) * 31 + 2);
}

TEST_CASE("reflex::hash: meta::info is hashable")
{
  static_assert(hashable_c<std::meta::info>);
  static_assert(hash(^^user::point) == hash("user::point"sv));
}
