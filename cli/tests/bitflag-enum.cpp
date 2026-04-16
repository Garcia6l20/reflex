#include <bitflag_cli.hpp>

using namespace reflex;

int main(int argc, const char* argv[]) {
  return cli::run<bitflag_enum_cli>(argc, argv);
}
