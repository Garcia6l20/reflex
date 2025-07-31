#include <reflex/serialize.hpp>

#include <print>

using namespace reflex;
namespace json = reflex::serialize::json;

#include <reflex/testing_main.hpp>

namespace serialize_tests {

  void test_int_deserialization()
  {
    auto v = json::load("42");
    check_that(v.has_value<json::number>());
    check_that(v) == json::number{42};
  }
  void test_string_deserialization()
  {
    auto v = json::load("\"42\"");
    check_that(v.has_value<json::string>());
    check_that(v) == json::string{"42"};
  }
  void test_bool_deserialization()
  {
    check_that(json::load("true") == true);
    check_that(json::load("false") == false);
  }
  void test_vec_deserialization()
  {
    auto v = json::load("[1,2,3]");
    check_that(v.has_value<json::array>());
    check_that(v[0]) == 1.0;
    check_that(v[1]) == 2.0;
    check_that(v[2]) == 3.0;
  }
  void test_obj_deserialization()
  {
    auto v = json::load(R"({"a": 1, "b": 2, "c": 3})");
    check_that(v.has_value<json::object>());
    check_that(v["a"]) == 1.0;
    check_that(v["b"]) == 2.0;
    check_that(v["c"]) == 3.0;
  }
}
