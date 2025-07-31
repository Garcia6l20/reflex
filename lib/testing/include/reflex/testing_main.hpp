#pragma once

#include <reflex/testing.hpp>
#include <reflex/testing/test_suite.hpp>

namespace reflex::testing {
  int main(int argc, const char** argv, test_suite const&);
}

int main(int argc, const char** argv)
{
  using namespace reflex;
  using namespace reflex::testing;
  return reflex::testing::main(argc, argv, test_suite::parse<^^::>());
}
