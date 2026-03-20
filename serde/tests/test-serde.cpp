#include <doctest/doctest.h>

#include <reflex/serde.hpp>

using namespace reflex;
using namespace reflex::serde;
using namespace std::string_view_literals;

TEST_CASE("serde::serialized_name")
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
    [[= reflex::serde::rename{"memberTwoRenamed"}]] int memberTwo;
  };

  CHECK_EQ(serialized_name(^^S3::memberOne), "member-one"sv);
  CHECK_EQ(serialized_name(^^S3::memberTwo), "memberTwoRenamed"sv);
}

// TEST_CASE("serde::members_of")
// {
//   struct[[= naming::camel_case]] S
//   {
//     int                                    int_member;
//     std::string                            string_member;
//     [[= naming::kebab_case]] double double_member;
//   };

//   template for(constexpr auto m : define_static_array(
//                    std::meta::nonstatic_data_members_of(^^S, std::meta::access_context::current())))
//   {
//     std::println("Member: {}", identifier_of(m));
//     std::println("Serialized: {}", serde::serialized_name(m));
//   }

//   static constexpr auto members = define_static_array(serde::members_of(^^S));
//   static_assert(members.size() == 3);
//   static_assert(*members[0].name == "intMember");
//   static_assert(*members[1].name == "stringMember");
//   static_assert(*members[2].name == "double-member");
// }
