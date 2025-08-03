#include <reflex/serialize.hpp>

#include <print>

using namespace reflex;
namespace json = reflex::serialize::json;

#include <reflex/testing_main.hpp>

namespace serialize_tests {

  void test_int_deserialization()
  {
    auto v = json::load("42");
    CHECK_THAT(v.has_value<json::number>());
    CHECK_THAT(v) == json::number{42};
  }
  void test_string_deserialization()
  {
    auto v = json::load("\"42\"");
    CHECK_THAT(v.has_value<json::string>());
    CHECK_THAT(v) == json::string{"42"};
  }
  void test_bool_deserialization()
  {
    CHECK_THAT(json::load("true") == true);
    CHECK_THAT(json::load("false") == false);
  }
  void test_vec_deserialization()
  {
    auto v = json::load("[1,2,3]");
    CHECK_THAT(v.has_value<json::array>());
    CHECK_THAT(v[0]) == 1.0;
    CHECK_THAT(v[1]) == 2.0;
    CHECK_THAT(v[2]) == 3.0;
  }
  void test_obj_deserialization()
  {
    auto v = json::load(R"({"a": 1, "b": 2, "c": 3})");
    CHECK_THAT(v.has_value<json::object>());
    CHECK_THAT(v["a"]) == 1.0;
    CHECK_THAT(v["b"]) == 2.0;
    CHECK_THAT(v["c"]) == 3.0;
  }
}
