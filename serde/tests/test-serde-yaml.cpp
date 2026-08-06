#include <doctest/doctest.h>

import reflex.serde.yaml;
import serde.tests.types;

import std;

using namespace reflex;
using namespace reflex::serde;
using namespace std::literals;

// The bulk-scan cliff, pinned. An in-memory input is a contiguous cursor, a
// stream cursor is not. The yaml parser is written to be correct on both; this
// is what makes the two runs of every later test case genuinely different code
// if a bulk fast path is ever added.
static_assert(yaml::deserializer<std::string_view::const_iterator>::bulk_scan);
static_assert(not yaml::deserializer<std::istreambuf_iterator<char>>::bulk_scan);

// The registry is namespace reflection over ^^reflex::serde::ser, so a backend
// is only reachable from with_serializer() when the program *links* it, not
// merely when it imports it. That distinction has bitten this repo before, so
// it is pinned rather than assumed.
TEST_CASE("yaml is registered as a serializer")
{
  bool found = false;
  template for(constexpr auto entry : define_static_array(serde::serializers()))
  {
    if(identifier_of(entry) == "yaml")
    {
      found = true;
    }
  }
  CHECK(found);
}

TEST_CASE("yaml is registered as a deserializer")
{
  bool found = false;
  template for(constexpr auto entry : define_static_array(serde::deserializers()))
  {
    if(identifier_of(entry) == "yaml")
    {
      found = true;
    }
  }
  CHECK(found);
}
