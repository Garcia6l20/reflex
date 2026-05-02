#include <doctest/doctest.h>

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
