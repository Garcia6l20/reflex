#include <reflex/meta.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace reflex;

template <auto... Values> struct templated_annotation_t
{
};

template <auto... Values> constexpr templated_annotation_t<Values...> templated_annotation;

#define dump(...) std::println("> {} = {}", #__VA_ARGS__, __VA_ARGS__);

TEST_CASE("simple queries")
{
  SECTION("simple annotations")
  {
    static constexpr struct
    {
    } a1;
    static constexpr struct
    {
    } a2;

    struct S1
    {
      [[= a1]] bool        a;
      [[= a1]] int         b;
      [[= a2]] std::string c;
      [[= a2]] std::string d;
    };

    template for(constexpr auto a : define_static_array(annotations_of(^^S1::a)))
    {
      constexpr auto c  = constant_of(a);
      constexpr auto eq = c == constant_of(^^a1);
      std::println("{} {} {}", display_string_of(c), display_string_of(^^a1), eq);
      // std::println("{}", );
    }
    STATIC_REQUIRE(meta::has_annotation(^^S1::a, ^^a1));
    {
      constexpr auto members = define_static_array(meta::nonstatic_data_members_annotated_with(^^S1, ^^a1));
      STATIC_REQUIRE(members.size() == 2);
      STATIC_REQUIRE(members[0] == ^^S1::a);
      STATIC_REQUIRE(members[1] == ^^S1::b);
    }
    {
      constexpr auto members = define_static_array(meta::nonstatic_data_members_annotated_with(^^S1, ^^a2));
      STATIC_REQUIRE(members.size() == 2);
      STATIC_REQUIRE(members[0] == ^^S1::c);
      STATIC_REQUIRE(members[1] == ^^S1::d);
    }
  }
  SECTION("templated annotations")
  {
    struct S1
    {
      bool                                           a;
      [[= templated_annotation<42>]] int             b;
      std::string                                    c;
      [[= templated_annotation<51>]] std::string     d;
      [[= templated_annotation<^^int>]] std::string  e;
      [[= templated_annotation<^^bool>]] std::string f;
    };
    dump(is_template(^^templated_annotation_t));
    // std::println("{} {}", display_string_of(c), eq);
    template for(constexpr auto a : define_static_array(annotations_of(^^S1::b)))
    {
      constexpr auto c  = template_of(type_of(a));
      constexpr auto eq = c == ^^templated_annotation_t;
      std::println("{} {}", display_string_of(c), eq);
      // std::println("{}", );
    }
    {
      constexpr auto members =
          define_static_array(meta::nonstatic_data_members_annotated_with(^^S1, ^^templated_annotation_t));
      STATIC_REQUIRE(members.size() == 4);
      STATIC_REQUIRE(members[0] == ^^S1::b);
      STATIC_REQUIRE(members[1] == ^^S1::d);
      STATIC_REQUIRE(members[2] == ^^S1::e);
      STATIC_REQUIRE(members[3] == ^^S1::f);
    }
    {
      constexpr auto member = meta::first_nonstatic_data_member_annotated_with(^^S1, ^^templated_annotation<51>);
      STATIC_REQUIRE(member != meta::null);
      STATIC_REQUIRE(member == ^^S1::d);
    }
  }
}
