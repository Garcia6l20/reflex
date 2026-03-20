import reflex.shell;
import git;
import std;

using namespace reflex;

int main(int argc, const char* argv[]) {
  return shell::run(git{}, argc, argv);
}
