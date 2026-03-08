#include <doctest/doctest.h>

import reflex.core;

using namespace reflex;

template <auto... Values> struct templated_annotation_t
{
};

template <auto... Values> constexpr templated_annotation_t<Values...> templated_annotation;

#define dump(...) std::println("> {} = {}", #__VA_ARGS__, __VA_ARGS__);

static constexpr struct _a1
{
} a1;
static constexpr struct _a2
{
} a2;

template <int V> struct _ta
{
  static constexpr int value = V;
};
template <int V> static constexpr _ta<V> ta;

TEST_CASE("reflex::meta::has_annotation")
{
  struct S1
  {
    [[= a1]] bool            a;
    [[= a1]] int             b;
    [[= a2]] std::string     c;
    [[= a2]] std::string     d;
    [[= ta<42>]] std::string e;
    [[= ta<55>]] std::string f;
  };

  static_assert(meta::has_annotation(^^S1::a, ^^_a1));
  static_assert(meta::has_annotation(^^S1::a, ^^a1));
  static_assert(meta::has_annotation(^^S1::b, ^^a1));
  static_assert(meta::has_annotation(^^S1::e, ^^_ta));
  static_assert(meta::has_annotation(^^S1::e, ^^ta<42>));

  {
    constexpr auto members =
        define_static_array(meta::nonstatic_data_members_annotated_with(^^S1, ^^a1));
    // auto checker = STATIC_CHECK_THAT(members.size());
    // std::println("{}", display_string_of(type_of(^^checker)));
    static_assert(members.size() == 2);
    static_assert(members[0] == ^^S1::a);
    static_assert(members[1] == ^^S1::b);
  }
  {
    constexpr auto members =
        define_static_array(meta::nonstatic_data_members_annotated_with(^^S1, ^^a2));
    static_assert(members.size() == 2);
    static_assert(members[0] == ^^S1::c);
    static_assert(members[1] == ^^S1::d);
  }
  {
    constexpr auto members =
        define_static_array(meta::nonstatic_data_members_annotated_with(^^S1, ^^_ta));
    static_assert(members.size() == 2);
    static_assert(members[0] == ^^S1::e);
    static_assert(members[1] == ^^S1::f);
  }
  {
    constexpr auto members =
        define_static_array(meta::nonstatic_data_members_annotated_with(^^S1, ^^ta<42>));
    static_assert(members.size() == 1);
  }
}

TEST_CASE("reflex::meta::has_templated_annotation")
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
    constexpr auto members = define_static_array(
        meta::nonstatic_data_members_annotated_with(^^S1, ^^templated_annotation_t));
    static_assert(members.size() == 4);
    static_assert(members[0] == ^^S1::b);
    static_assert(members[1] == ^^S1::d);
    static_assert(members[2] == ^^S1::e);
    static_assert(members[3] == ^^S1::f);
  }
  {
    constexpr auto member =
        meta::first_nonstatic_data_member_annotated_with(^^S1, ^^templated_annotation<51>);
    static_assert(member != meta::null);
    static_assert(member == ^^S1::d);
  }
}
