#pragma once

#include <reflex/testing/main_impl.hpp>

int main(int argc, const char** argv)
{
  reflex::testing::command_line<^^::> cli{};
  return cli.run(argc, argv);
}
