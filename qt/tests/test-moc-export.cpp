#include "moc-pair.hpp"

#include <reflex/qt/moc/export.hpp>

#include <doctest/doctest.h>

#include <fcntl.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace moc = reflex::qt::moc;

REFLEX_QT_MODULE(pair_types, m)
{
  m.expose<twin>();
  m.expose<twin_qml>();
  m.expose<twin_gadget>();
}

namespace
{
auto contents_of(std::filesystem::path const& path) -> std::string
{
  std::ostringstream text;
  text << std::ifstream{path}.rdbuf();
  return text.str();
}

struct sandbox
{
  sandbox() : previous{std::filesystem::current_path()}
  {
    auto pattern = (std::filesystem::temp_directory_path() / "reflex-qt-export-XXXXXX").string();
    std::vector<char> buffer(pattern.begin(), pattern.end());
    buffer.push_back('\0');
    REQUIRE(::mkdtemp(buffer.data()) != nullptr);
    dir = std::filesystem::path{buffer.data()};
  }

  ~sandbox()
  {
    std::error_code ignored;
    std::filesystem::current_path(previous, ignored);
    std::filesystem::remove_all(dir, ignored);
  }

  sandbox(sandbox const&)                    = delete;
  auto operator=(sandbox const&) -> sandbox& = delete;

  void enter() const { std::filesystem::current_path(dir); }

  std::filesystem::path previous;
  std::filesystem::path dir;
};

struct redirected
{
  redirected(int descriptor, std::filesystem::path capture)
      : fd{descriptor}, path{std::move(capture)}, saved{::dup(descriptor)}
  {
    REQUIRE(saved >= 0);
    const int sink = ::open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    REQUIRE(sink >= 0);
    ::dup2(sink, fd);
    ::close(sink);
  }

  ~redirected()
  {
    ::dup2(saved, fd);
    ::close(saved);
    std::filesystem::remove(path);
  }

  redirected(redirected const&)                    = delete;
  auto operator=(redirected const&) -> redirected& = delete;

  int                   fd;
  std::filesystem::path path;
  int                   saved;
};

struct outcome
{
  int         status;
  std::string out;
  std::string err;
};

auto run(std::vector<std::string> arguments) -> outcome
{
  std::vector<char*> argv;
  for(auto& argument : arguments)
  {
    argv.push_back(argument.data());
  }

  const auto own  = std::to_string(::getpid());
  const auto base = std::filesystem::temp_directory_path();
  outcome    result{};

  std::fflush(nullptr);
  {
    const redirected err{STDERR_FILENO, base / ("reflex-qt-export-" + own + "-err")};
    const redirected out{STDOUT_FILENO, base / ("reflex-qt-export-" + own + "-out")};

    result.status = moc::export_main<pair_types>(static_cast<int>(argv.size()), argv.data());

    std::fflush(nullptr);
    std::cout.flush();
    result.out = contents_of(out.path);
    result.err = contents_of(err.path);
  }
  return result;
}
} // namespace

TEST_CASE("a trailing bare -I is an error, not an empty include root")
{
  const sandbox box;
  box.enter();

  const auto result = run({"exporter", "out.json", "-I"});

  CHECK_EQ(result.status, EXIT_FAILURE);
  CHECK(result.err.contains("-I needs a directory"));
  CHECK_FALSE(std::filesystem::exists(box.dir / "out.json"));
}

TEST_CASE("an empty -I value is an error")
{
  const sandbox box;
  box.enter();

  const auto result = run({"exporter", "-I", "", "out.json"});

  CHECK_EQ(result.status, EXIT_FAILURE);
  CHECK(result.err.contains("-I needs a directory"));
  CHECK_FALSE(std::filesystem::exists(box.dir / "out.json"));
}

TEST_CASE("an unknown option is an error, not the output path")
{
  const sandbox box;
  box.enter();

  const auto result = run({"exporter", "--help"});

  CHECK_EQ(result.status, EXIT_FAILURE);
  CHECK(result.err.contains("unknown option --help"));
  CHECK_FALSE(std::filesystem::exists(box.dir / "--help"));
}

TEST_CASE("a second positional is an error, and neither is written")
{
  const sandbox box;
  box.enter();

  const auto result = run({"exporter", "p1.json", "p2.json"});

  CHECK_EQ(result.status, EXIT_FAILURE);
  CHECK(result.err.contains("more than one output path"));
  CHECK_FALSE(std::filesystem::exists(box.dir / "p1.json"));
  CHECK_FALSE(std::filesystem::exists(box.dir / "p2.json"));
}

TEST_CASE("a second -C is an error")
{
  const sandbox box;
  box.enter();

  const auto result = run({"exporter", "out.json", "-C", "one", "-C", "two"});

  CHECK_EQ(result.status, EXIT_FAILURE);
  CHECK(result.err.contains("more than one -C directory"));
}

TEST_CASE("no output path is an error naming the usage")
{
  const auto result = run({"exporter"});

  CHECK_EQ(result.status, EXIT_FAILURE);
  CHECK(result.err.contains("no output path"));
  CHECK(result.err.contains("usage: exporter"));
}

TEST_CASE("an unwritable output path is reported with the path and the reason")
{
  const sandbox box;
  const auto    output = box.dir / "absent" / "out.json";

  const auto result = run({"exporter", output.string(), "-C", box.previous.string()});

  CHECK_EQ(result.status, EXIT_FAILURE);
  CHECK(result.err.contains(output.string()));
  CHECK(result.err.contains("No such file or directory"));
}

TEST_CASE("a relative recorded path with no -C is an error, and nothing is written")
{
  const sandbox box;

  const auto result = run({"exporter", (box.dir / "out.json").string()});

  CHECK_EQ(result.status, EXIT_FAILURE);
  CHECK(result.err.contains("moc-pair.hpp"));
  CHECK(result.err.contains("the compiler recorded a relative path"));
  CHECK(result.err.contains("-C"));
  CHECK_FALSE(std::filesystem::exists(box.dir / "out.json"));
}

TEST_CASE("a -C that does not complete the recorded path is an error naming the path")
{
  const sandbox box;

  const auto result = run({"exporter", (box.dir / "out.json").string(), "-C", box.dir.string()});

  CHECK_EQ(result.status, EXIT_FAILURE);
  CHECK(result.err.contains((box.dir / "../qt/tests/moc-pair.hpp").generic_string()));
  CHECK(result.err.contains("no such file"));
  CHECK_FALSE(std::filesystem::exists(box.dir / "out.json"));
}

TEST_CASE("-C makes the document the same from any working directory")
{
  const sandbox box;
  const auto    root   = box.previous.string();
  const auto    here   = box.dir / "here.json";
  const auto    there  = box.dir / "there.json";
  const auto    higher = box.dir / "higher.json";

  REQUIRE_EQ(run({"exporter", here.string(), "-C", root}).status, EXIT_SUCCESS);

  box.enter();
  REQUIRE_EQ(run({"exporter", there.string(), "-C", root}).status, EXIT_SUCCESS);

  std::filesystem::current_path(std::filesystem::temp_directory_path());
  REQUIRE_EQ(run({"exporter", higher.string(), "-C" + root}).status, EXIT_SUCCESS);

  CHECK_EQ(contents_of(here), contents_of(there));
  CHECK_EQ(contents_of(here), contents_of(higher));
  CHECK(contents_of(here).contains("moc-pair.hpp"));
}

TEST_CASE("-C and -I together produce the relocatable spelling from any directory")
{
  const auto full = moc::metatypes_of<pair_types>().front().inputFile;
  const auto tree = std::filesystem::path{full}.parent_path().parent_path().string();

  const sandbox box;
  const auto    root   = box.previous.string();
  const auto    output = box.dir / "out.json";

  box.enter();
  const auto result = run({"exporter", output.string(), "-C", root, "-I", tree});

  REQUIRE_EQ(result.status, EXIT_SUCCESS);
  CHECK(contents_of(output).contains(R"("inputFile":"tests/moc-pair.hpp")"));
}

TEST_CASE("- writes the document to stdout")
{
  const auto result = run({"exporter", "-", "-C", std::filesystem::current_path().string()});

  CHECK_EQ(result.status, EXIT_SUCCESS);
  CHECK(result.err.empty());
  CHECK(result.out.contains(R"("qualifiedClassName":"twin")"));
}
