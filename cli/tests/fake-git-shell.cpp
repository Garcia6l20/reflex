#include <reflex/shell.hpp>

#include <git.hpp>

using namespace reflex;

int main(int argc, const char* argv[]) {
  return shell::run(git{}, argc, argv);
}
