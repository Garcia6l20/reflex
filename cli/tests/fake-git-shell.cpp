#define MODULE_MODE

#include <git.hpp>

import reflex.shell;

using namespace reflex;

int main(int argc, const char* argv[]) {
  return shell::run(git{}, argc, argv);
}
