import reflex.cli;
import git;
import std;

using namespace reflex;

int main(int argc, const char* argv[]) {
  return cli::run(git{}, argc, argv);
}