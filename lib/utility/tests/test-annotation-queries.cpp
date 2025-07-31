#include <reflex/meta.hpp>

#include <reflex/testing_main.hpp>

using namespace reflex;

template <auto... Values> struct templated_annotation_t
{
};

template <auto... Values> constexpr templated_annotation_t<Values...> templated_annotation;

#define dump(...) std::println("> {} = {}", #__VA_ARGS__, __VA_ARGS__);

namespace annotation_queries_tests
{

void test_simple_annotations()
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

  check_that(meta::has_annotation(^^S1::a, ^^a1));
  {
    constexpr auto members = define_static_array(meta::nonstatic_data_members_annotated_with(^^S1, ^^a1));
    // auto checker = static_check_that(members.size());
    // std::println("{}", display_string_of(type_of(^^checker)));
    static_check_that(members.size()) == 2;
    static_check_that(members[0] == ^^S1::a);
    static_check_that(members[1] == ^^S1::b);
  }
  {
    constexpr auto members = define_static_array(meta::nonstatic_data_members_annotated_with(^^S1, ^^a2));
    static_check_that(members.size() == 2);
    static_check_that(members[0] == ^^S1::c);
    static_check_that(members[1] == ^^S1::d);
  }
}
void test_templated_annotations()
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
    static_check_that(members.size() == 4);
    static_check_that(members[0] == ^^S1::b);
    static_check_that(members[1] == ^^S1::d);
    static_check_that(members[2] == ^^S1::e);
    static_check_that(members[3] == ^^S1::f);
  }
  {
    constexpr auto member = meta::first_nonstatic_data_member_annotated_with(^^S1, ^^templated_annotation<51>);
    static_check_that(member != meta::null);
    static_check_that(member == ^^S1::d);
  }
}
} // namespace annotation_queries_tests
