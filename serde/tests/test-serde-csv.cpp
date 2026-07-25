#include <doctest/doctest.h>

import reflex.serde.csv;
import serde.tests.types;

import std;

using namespace reflex;
using namespace reflex::serde;
using namespace std::literals;

struct[[= serde::naming::camel_case, = derive(Debug)]] Row
{
  int                                    int_member;
  std::string                            string_member;
  [[= serde::naming::kebab_case]] double double_member;

  constexpr bool operator==(Row const& other) const = default;
};

TEST_CASE("reflex::serde::csv::serializer: single row")
{
  std::string      out;
  csv::serializer  ser{out};
  ser.dump(Row{42, "hello", 3.14});
  CHECK_EQ(out, "intMember,stringMember,double-member\r\n42,hello,3.14\r\n");
}

TEST_CASE("reflex::serde::csv::serializer: sequence of rows")
{
  std::string     out;
  csv::serializer ser{out};
  std::vector<Row> rows{
      {1, "a", 1.5},
      {2, "b", 2.5},
  };
  ser.dump(rows);
  CHECK_EQ(out, "intMember,stringMember,double-member\r\n1,a,1.5\r\n2,b,2.5\r\n");
}

TEST_CASE("reflex::serde::csv::serializer: RFC 4180 quoting")
{
  std::string     out;
  csv::serializer ser{out};
  ser.dump(Row{7, "a,b\"c\nd", 0.0});
  CHECK_EQ(out, "intMember,stringMember,double-member\r\n7,\"a,b\"\"c\nd\",0\r\n");
}

TEST_CASE("reflex::serde::csv::deserializer: single row")
{
  const std::string_view in = "intMember,stringMember,double-member\r\n42,hello,3.14\r\n";
  const auto             value = csv::deserializer{in}.load<Row>();
  CHECK_EQ(value, Row{42, "hello", 3.14});
}

TEST_CASE("reflex::serde::csv::deserializer: sequence of rows")
{
  const std::string_view in = "intMember,stringMember,double-member\r\n1,a,1.5\r\n2,b,2.5\r\n";
  const auto             value = csv::deserializer{in}.load<std::vector<Row>>();
  REQUIRE_EQ(value.size(), 2u);
  CHECK_EQ(value[0], Row{1, "a", 1.5});
  CHECK_EQ(value[1], Row{2, "b", 2.5});
}

TEST_CASE("reflex::serde::csv: roundtrip with quoting")
{
  const std::vector<Row> expected{
      {7, "a,b\"c\nd", 1.0},
      {8, "plain", 2.0},
  };
  std::string     out;
  csv::serializer ser{out};
  ser.dump(expected);

  const auto value = csv::deserializer{out}.load<std::vector<Row>>();
  CHECK_EQ(value, expected);
}

TEST_CASE("reflex::serde::csv: header name parity with naming annotation")
{
  std::string     out;
  csv::serializer ser{out};
  ser.dump(Row{1, "x", 1.0});
  // camel_case + kebab_case overrides match what JSON emits as object keys
  CHECK(out.starts_with("intMember,stringMember,double-member\r\n"));
}

TEST_CASE("reflex::serde::csv: optional field")
{
  SUBCASE("present")
  {
    std::string     out;
    csv::serializer ser{out};
    ser.dump(Opt{"a", 5});
    CHECK_EQ(out, "name,count\r\na,5\r\n");
    const auto value = csv::deserializer{std::string_view{out}}.load<Opt>();
    CHECK_EQ(value, Opt{"a", 5});
  }
  SUBCASE("absent -> empty cell")
  {
    std::string     out;
    csv::serializer ser{out};
    ser.dump(Opt{"a", std::nullopt});
    CHECK_EQ(out, "name,count\r\na,\r\n");
    const auto value = csv::deserializer{std::string_view{out}}.load<Opt>();
    CHECK_EQ(value, Opt{"a", std::nullopt});
  }
}

TEST_CASE("reflex::serde::csv: enum field roundtrip")
{
  std::string     out;
  csv::serializer ser{out};
  ser.dump(Enumed{"x", Color::Green});
  CHECK_EQ(out, "name,color\r\nx,Green\r\n");
  const auto value = csv::deserializer{std::string_view{out}}.load<Enumed>();
  CHECK_EQ(value, Enumed{"x", Color::Green});
}

TEST_CASE("reflex::serde::csv: file roundtrip")
{
  const std::filesystem::path csv_path = "test.csv";
  const std::vector<Row>      expected{
      {42, "hello", 3.14},
      {7,  "world", 2.71},
  };
  {
    std::ofstream   out_file{csv_path};
    csv::serializer ser{out_file};
    ser.dump(expected);
  }
  {
    std::ifstream in_file{csv_path};
    const auto    value = csv::deserializer{in_file}.load<std::vector<Row>>();
    CHECK_EQ(value, expected);
  }
  std::filesystem::remove(csv_path);
}

// Compile-time rejection of non-scalar fields: a struct with a nested aggregate
// or a container member is not a csv_row_c.
struct Nested
{
  Row r;
};
struct WithVector
{
  std::vector<int> xs;
};
static_assert(not csv::csv_row_c<Nested>);
static_assert(not csv::csv_row_c<WithVector>);
static_assert(csv::csv_row_c<Row>);
