#include <git.hpp>

using namespace reflex;

int main(int argc, const char* argv[]) {
  return cli::run(git{}, argc, argv);
}