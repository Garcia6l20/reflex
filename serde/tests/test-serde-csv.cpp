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

TEST_CASE("reflex::serde::csv::deserializer: read_record borrows from the input")
{
  const std::string_view in = "a,b\r\nplain,\"quo\"\"ted\"\r\n";
  csv::deserializer      de{in};

  auto header = de.read_record();
  REQUIRE(header.has_value());
  REQUIRE_EQ(header->size(), 2u);
  // A field that needed no decoding points into the input buffer itself.
  CHECK_EQ((*header)[0].data(), in.data());

  auto record = de.read_record();
  REQUIRE(record.has_value());
  REQUIRE_EQ(record->size(), 2u);
  CHECK_EQ((*record)[0], "plain"sv);
  CHECK_EQ((*record)[0].data(), in.data() + 5);
  // A field that had to be decoded is materialized, so it does not alias the
  // input, but it must still read correctly.
  CHECK_EQ((*record)[1], "quo\"ted"sv);

  // End of input, and the fields of the last record are still readable until the
  // next call returns nullopt.
  CHECK_FALSE(de.read_record().has_value());
}

// Header-to-member resolution. These pin the precedence rules an index table can
// silently invert, so they are worth having whatever the mapping is implemented
// as. Row's members are int_member / string_member / double_member, serialized as
// intMember / stringMember / double-member.
TEST_CASE("reflex::serde::csv::deserializer: header to member precedence")
{
  SUBCASE("a duplicated column assigns twice, so the last one wins")
  {
    const std::string_view in = "intMember,intMember\r\n1,2\r\n";
    CHECK_EQ(csv::deserializer{in}.load<Row>().int_member, 2);
  }
  SUBCASE("only the serialized name is accepted, not the C++ identifier")
  {
    // A member is read under the name it is written under. int_member is
    // serialized as intMember, so a header spelling the identifier names no
    // column this row has.
    CHECK_EQ(csv::deserializer{"intMember\r\n5\r\n"sv}.load<Row>().int_member, 5);
    CHECK_EQ(csv::deserializer{"int_member\r\n5\r\n"sv}.load<Row>().int_member, 0);
    // The identifier spelling is an unknown column, so it is ignored and does
    // not shift the cell the serialized name resolves to.
    CHECK_EQ(csv::deserializer{"int_member,intMember\r\n1,2\r\n"sv}.load<Row>().int_member, 2);
  }
  SUBCASE("a column matching no member is ignored, and does not shift the rest")
  {
    const std::string_view in = "nope,intMember\r\nxxx,7\r\n";
    CHECK_EQ(csv::deserializer{in}.load<Row>().int_member, 7);
  }
  SUBCASE("a member with no column keeps its default")
  {
    const auto v = csv::deserializer{"intMember\r\n7\r\n"sv}.load<Row>();
    CHECK_EQ(v.int_member, 7);
    CHECK_EQ(v.string_member, "");
    CHECK_EQ(v.double_member, 0.0);
  }
  SUBCASE("fewer cells than header columns leaves the tail defaulted")
  {
    const std::string_view in = "intMember,stringMember,double-member\r\n7,hi\r\n";
    const auto             v  = csv::deserializer{in}.load<Row>();
    CHECK_EQ(v.int_member, 7);
    CHECK_EQ(v.string_member, "hi");
    CHECK_EQ(v.double_member, 0.0);
  }
  SUBCASE("more cells than header columns ignores the extras")
  {
    CHECK_EQ(csv::deserializer{"intMember\r\n7,ignored\r\n"sv}.load<Row>().int_member, 7);
  }
  SUBCASE("columns in an order other than declaration order")
  {
    const std::string_view in = "double-member,intMember,stringMember\r\n1.5,7,hi\r\n";
    CHECK_EQ(csv::deserializer{in}.load<Row>(), Row{7, "hi", 1.5});
  }
  SUBCASE("the same rules apply per row for a sequence")
  {
    const std::string_view in = "nope,intMember,intMember\r\na,1,2\r\nb,3,4\r\n";
    const auto             v  = csv::deserializer{in}.load<std::vector<Row>>();
    REQUIRE_EQ(v.size(), 2u);
    CHECK_EQ(v[0].int_member, 2);
    CHECK_EQ(v[1].int_member, 4);
  }
}

// The contiguous fast path is what makes read_record hand out views. Pin both
// halves so a future change to the cursor cannot silently move CSV onto the
// character-at-a-time path, or silently change what read_record owns.
static_assert(csv::deserializer<std::string_view::const_iterator>::bulk_scan);
static_assert(std::same_as<csv::deserializer<std::string_view::const_iterator>::field_str,
                           std::string_view>);
static_assert(not csv::deserializer<std::istreambuf_iterator<char>>::bulk_scan);
static_assert(std::same_as<csv::deserializer<std::istreambuf_iterator<char>>::field_str,
                           std::string>);

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
// A std::string_view member reads a view of the input instead of a copy.
//
// The borrow is only offered for a cell that is a run of the input. A cell that
// had to be decoded, which for CSV means a doubled quote, is parked in a pool
// read_record clears on the next record, so it throws rather than handing out a
// view that goes stale a record later. On the streaming cursor there is no
// buffer at all and it is a compile error:
//
//   csv::deserializer{std::istringstream{...}}.load<BorrowedRow>();
//
//   error: static assertion failed: std::basic_string_view<char> cannot be a CSV
//   string destination on this cursor: a borrowed read needs a contiguous input
//   to point at, and this deserializer reads a character at a time (use
//   std::string, or deserialize from a contiguous input)
struct BorrowedRow
{
  std::string_view text;
  int              n;
};

TEST_CASE("reflex::serde::csv: a borrowed string destination")
{
  SUBCASE("an un-decoded cell points into the input")
  {
    // Held by name: the view read out of it points into it.
    const std::string in = "text,n\r\nplain,1\r\n";
    const auto        v  = csv::deserializer{std::string_view{in}}.load<BorrowedRow>();
    CHECK_EQ(v.text, "plain"sv);
    CHECK_EQ(v.n, 1);
    // Address, not content: comparing bytes would pass for a copy too.
    CHECK_EQ(v.text.data(), in.data() + in.find("plain"));
  }

  SUBCASE("a quoted cell with nothing to decode still borrows")
  {
    // Quoting alone does not break the run. Only the doubled quote does, because
    // that is what forces a byte to be dropped from the middle.
    const std::string in = "text,n\r\n\"a,b\",1\r\n";
    const auto        v  = csv::deserializer{std::string_view{in}}.load<BorrowedRow>();
    CHECK_EQ(v.text, "a,b"sv);
    CHECK_EQ(v.text.data(), in.data() + in.find("a,b"));
  }

  SUBCASE("a decoded cell throws rather than going stale")
  {
    const std::string in = "text,n\r\n\"qu\"\"oted\",1\r\n";
    CHECK_THROWS_AS(
        (csv::deserializer{std::string_view{in}}.load<BorrowedRow>()), std::runtime_error);
  }

  // The failure the throw exists to prevent, kept as the reason it is a throw and
  // not a copy into the pool. With the guard removed, three decoded cells across
  // three records all read back the last one: read_record clears the pool, the
  // next decoded cell reuses the slot, and every earlier view follows it.
  SUBCASE("decoded cells across records would otherwise alias each other")
  {
    const std::string in = "text,n\r\n\"aa\"\"aa\",1\r\n\"bb\"\"bb\",2\r\n\"cc\"\"cc\",3\r\n";
    CHECK_THROWS_AS(
        (csv::deserializer{std::string_view{in}}.load<std::vector<BorrowedRow>>()),
        std::runtime_error);
  }

  SUBCASE("an empty cell borrows nothing and is fine")
  {
    const std::string in = "text,n\r\n,1\r\n";
    const auto        v  = csv::deserializer{std::string_view{in}}.load<BorrowedRow>();
    CHECK(v.text.empty());
    CHECK_EQ(v.n, 1);
  }

  // The boundary that matters, asserted rather than assumed. read_record clears
  // the pool, so a decoded cell would go stale here. An un-decoded one does not:
  // it is the input's, and the input has not moved. Reading every row of a
  // sequence into borrowed views is the case this has to hold for.
  SUBCASE("a borrow survives the records that follow it")
  {
    const std::string in = "text,n\r\nfirst,1\r\nsecond,2\r\nthird,3\r\n";
    const auto        rows = csv::deserializer{std::string_view{in}}.load<std::vector<BorrowedRow>>();
    REQUIRE_EQ(rows.size(), 3u);
    CHECK_EQ(rows[0].text, "first"sv);
    CHECK_EQ(rows[1].text, "second"sv);
    CHECK_EQ(rows[2].text, "third"sv);
    CHECK_EQ(rows[0].text.data(), in.data() + in.find("first"));
    CHECK_EQ(rows[1].text.data(), in.data() + in.find("second"));
    CHECK_EQ(rows[2].text.data(), in.data() + in.find("third"));
  }

  SUBCASE("and one decoded cell anywhere in the sequence throws")
  {
    const std::string in = "text,n\r\nfirst,1\r\n\"se\"\"cond\",2\r\n";
    CHECK_THROWS_AS(
        (csv::deserializer{std::string_view{in}}.load<std::vector<BorrowedRow>>()),
        std::runtime_error);
  }
}

TEST_CASE("reflex::serde::csv: an owning string destination is unaffected")
{
  struct Owned
  {
    std::string text;
    int         n;

    bool operator==(Owned const& other) const = default;
  };

  const std::string in = "text,n\r\nplain,1\r\n\"qu\"\"oted\",2\r\n";

  SUBCASE("both cells still copy and decode")
  {
    const auto rows = csv::deserializer{std::string_view{in}}.load<std::vector<Owned>>();
    REQUIRE_EQ(rows.size(), 2u);
    CHECK_EQ(rows[0].text, "plain");
    CHECK_EQ(rows[1].text, "qu\"oted");
    CHECK_NE(rows[0].text.data(), in.data() + in.find("plain"));
  }

  SUBCASE("and the streaming cursor agrees")
  {
    std::istringstream stream{in};
    CHECK_EQ(
        csv::deserializer{stream}.load<std::vector<Owned>>(),
        csv::deserializer{std::string_view{in}}.load<std::vector<Owned>>());
  }
}

static_assert(not csv::csv_row_c<Nested>);
static_assert(not csv::csv_row_c<WithVector>);
static_assert(csv::csv_row_c<Row>);
