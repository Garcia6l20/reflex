#include <doctest/doctest.h>

// ENOENT is a macro, so `import std;` does not bring it in
#include <cerrno>

import reflex.serde;

import std;

using namespace reflex;
using namespace reflex::serde;
using namespace std::string_view_literals;

TEST_CASE("reflex::serde::serialized_name")
{
  struct[[= naming::snake_case]] S1
  {
    int memberOne;
    int memberTwo;
  };

  CHECK_EQ(serialized_name(^^S1::memberOne), "member_one"sv);
  CHECK_EQ(serialized_name(^^S1::memberTwo), "member_two"sv);

  struct[[= naming::camel_case]] S2
  {
    int member_one;
    int member_two;
  };

  CHECK_EQ(serialized_name(^^S2::member_one), "memberOne"sv);
  CHECK_EQ(serialized_name(^^S2::member_two), "memberTwo"sv);

  struct[[= naming::camel_case]] S3
  {
    [[= naming::kebab_case]] int         memberOne;
    [[= rename{"memberTwoRenamed"}]] int memberTwo;
  };

  CHECK_EQ(serialized_name(^^S3::memberOne), "member-one"sv);
  CHECK_EQ(serialized_name(^^S3::memberTwo), "memberTwoRenamed"sv);
}

TEST_CASE("reflex::serde::mmap_input_stream")
{
  const std::filesystem::path path = "test-mmap-input-stream-core.txt";

  SUBCASE("exposes the file as a contiguous range of char")
  {
    {
      std::ofstream out_file{path, std::ios::binary};
      out_file << "abcdef";
    }
    mmap_input_stream in{path};
    CHECK_EQ(in.size(), 6u);
    CHECK_EQ(in.view(), "abcdef"sv);
    CHECK_EQ(in.end() - in.begin(), 6);
    CHECK_EQ(std::string(in.begin(), in.end()), "abcdef");
    static_assert(std::ranges::contiguous_range<mmap_input_stream>);
    static_assert(std::same_as<decltype(in.begin()), const char*>);
    std::filesystem::remove(path);
  }

  SUBCASE("an empty file is empty and still dereferenceable")
  {
    {
      std::ofstream out_file{path, std::ios::binary};
    }
    mmap_input_stream in{path};
    CHECK(in.empty());
    CHECK_EQ(in.size(), 0u);
    CHECK_NE(in.data(), nullptr);
    CHECK_EQ(in.begin(), in.end());
    std::filesystem::remove(path);
  }

  SUBCASE("a missing file throws filesystem_error carrying the path")
  {
    CHECK_THROWS_AS(mmap_input_stream{"no-such-file-here.txt"}, std::filesystem::filesystem_error);
    try
    {
      mmap_input_stream in{"no-such-file-here.txt"};
    }
    catch(std::filesystem::filesystem_error const& e)
    {
      CHECK_EQ(e.path1(), std::filesystem::path{"no-such-file-here.txt"});
      CHECK_EQ(e.code(), std::error_code(ENOENT, std::generic_category()));
    }
  }

  SUBCASE("a directory is not a document")
  {
    CHECK_THROWS_AS(mmap_input_stream{"."}, std::filesystem::filesystem_error);
  }

  SUBCASE("moving retargets the range at the new owner")
  {
    {
      std::ofstream out_file{path, std::ios::binary};
      out_file << "movable";
    }
    mmap_input_stream in{path};
    mmap_input_stream moved{std::move(in)};
    CHECK_EQ(moved.view(), "movable"sv);
    CHECK_EQ(std::string(moved.begin(), moved.end()), "movable");
    mmap_input_stream assigned{path};
    assigned = std::move(moved);
    CHECK_EQ(assigned.view(), "movable"sv);
    std::filesystem::remove(path);
  }

  static_assert(not std::copy_constructible<mmap_input_stream>);
  static_assert(std::move_constructible<mmap_input_stream>);
}
