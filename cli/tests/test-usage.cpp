#include <doctest/doctest.h>

#include <reflex/cli.hpp>
#include <testutils.hpp>

#include <sstream>
#include <string>

using namespace reflex;
using namespace testutils;

namespace
{
struct[[= cli::command{"Every switch spelling a usage line can hold."}]] spellings
{
  [[= cli::option{"-b", "A short spelling on its own."}.flag()]] //
  bool brief = false;

  [[= cli::option{"--verbose", "A long spelling on its own."}.flag()]] //
  bool verbose = false;

  [[= cli::option{"-p/--pair", "Both spellings."}.flag()]] //
  bool pair = false;

  int operator()() const
  {
    return 0;
  }
};

auto usage_of(const char** argv, int argc)
{
  int  rc  = -1;
  auto out = capture_out_err([&] { rc = cli::run(spellings{}, argc, argv); }).first;
  return std::pair{rc, out};
}

bool has_row(const std::string& out, std::string_view switches, std::string_view help)
{
  auto              ss = std::istringstream{out};
  const auto        head = "  " + std::string{switches} + " ";
  for(std::string line; std::getline(ss, line);)
  {
    if(line.starts_with(head) and line.ends_with(help))
    {
      return true;
    }
  }
  return false;
}
} // namespace

TEST_CASE("reflex::cli: usage names the program, not the path it was found at")
{
  const char* argv[]   = {"/opt/somewhere/spellings", "--help"};
  const auto [rc, out] = usage_of(argv, 2);
  CHECK_EQ(rc, 0);
  CHECK_NE(out.find("USAGE: spellings"), std::string::npos);
  CHECK_EQ(out.find("/opt/somewhere"), std::string::npos);
}

TEST_CASE("reflex::cli: a bare name is left alone")
{
  const char* argv[]   = {"spellings", "--help"};
  const auto [rc, out] = usage_of(argv, 2);
  CHECK_EQ(rc, 0);
  CHECK_NE(out.find("USAGE: spellings"), std::string::npos);
}

TEST_CASE("reflex::cli: each switch spelling gets its own usage row")
{
  const char* argv[]   = {"spellings", "--help"};
  const auto [rc, out] = usage_of(argv, 2);
  CHECK_EQ(rc, 0);
  CHECK(has_row(out, "-b", "A short spelling on its own."));
  CHECK(has_row(out, "--verbose", "A long spelling on its own."));
  CHECK(has_row(out, "-p/--pair", "Both spellings."));
}
