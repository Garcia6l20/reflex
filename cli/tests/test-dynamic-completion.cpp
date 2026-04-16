#include <doctest/doctest.h>

#include <reflex/cli.hpp>
#include <testutils.hpp>

#include <array>
#include <vector>

using namespace reflex;
using namespace testutils;

template <typename Cli>
std::vector<std::string>
    complete_with_mode(std::string_view mode, std::string_view comp_line, int comp_point)
{
  testutils::set_env("_REFLEX_COMPLETE", std::string{mode}.c_str(), true);
  testutils::set_env("_REFLEX_COMP_LINE", std::string{comp_line}.c_str(), true);
  testutils::set_env("_REFLEX_COMP_POINT", std::to_string(comp_point).c_str(), true);

  std::string out;
  {
    using namespace std::string_view_literals;
    const auto [o, _] =
        testutils::capture_out_err([] { cli::run(Cli{}, std::initializer_list{"cli"sv}); });
    out = o;
  }

  testutils::unset_env("_REFLEX_COMPLETE");
  testutils::unset_env("_REFLEX_COMP_LINE");
  testutils::unset_env("_REFLEX_COMP_POINT");

  std::vector<std::string> results;
  std::istringstream       ss{out};
  std::string              line;
  while(std::getline(ss, line))
  {
    if(not line.empty())
    {
      results.push_back(line);
    }
  }
  return results;
}

auto completion_values_from_stream(const std::vector<std::string>& lines)
{
  std::vector<std::string> values;
  for(std::size_t i = 0; i < lines.size();)
  {
    if(lines[i] == "0" or lines[i] == "1")
    {
      ++i;
      continue;
    }

    auto const type = parse<cli::completion_type>(lines[i]);
    if(not type.has_value() or i + 1 >= lines.size())
    {
      ++i;
      continue;
    }

    values.push_back(lines[i + 1]);
    i += 3;
  }
  return values;
}

struct[[= cli::command{"Simple echo command."}]] dynamic_completion_cli
{
  struct[[= cli::command{"Simple echo command."}]] test_command
  {
    enum class file_type
    {
      text,
      binary,
      archive
    };

    [[= cli::argument{"File type."}, = cli::completers::enumeration<file_type>{}]] file_type type =
        file_type::text;

    auto file_name_completer([[maybe_unused]] std::string_view current) const
    {
      constexpr std::array patterns{
          std::pair{file_type::text,    "*.txt"},
          std::pair{file_type::binary,  "*.bin"},
          std::pair{file_type::archive, "*.zip"},
      };
      const auto pattern = [&]() -> std::string_view {
        for(const auto& [ft, p] : patterns)
        {
          if(ft == type)
          {
            return p;
          }
        }
        return "*";
      }();
      return std::array{
          cli::completion<>{
                            .type        = cli::completion_type::file,
                            .value       = std::string(pattern),
                            .description = "File system path"}
      };
    }

    [[= cli::argument{"File name."}, = cli::complete{^^file_name_completer}]] std::string file_name;

    int operator()() const
    {
      std::println("Selected file type: {}, file name: {}", type, file_name);
      return 0;
    }
  } test{};
};

using namespace std::string_view_literals;

TEST_CASE("reflex::cli:dynamic completion")
{
  SUBCASE("nominal enum completion")
  {
    auto v = completion_values(complete<dynamic_completion_cli>("dynamic_completion_cli test ", 3))
           | std::views::transform(&cli::completion<>::value)
           | std::ranges::to<std::vector>();
    CHECK(std::ranges::contains(v, "text"sv));
    CHECK(std::ranges::contains(v, "binary"sv));
    CHECK(std::ranges::contains(v, "archive"sv));
  }
  SUBCASE("text type")
  {
    auto v =
        completion_values(complete<dynamic_completion_cli>("dynamic_completion_cli test text ", 4))
        | std::views::transform(&cli::completion<>::value)
        | std::ranges::to<std::vector>();
    CHECK(std::ranges::contains(v, "*.txt"sv));
  }
  SUBCASE("bin type")
  {
    auto v = completion_values(
                 complete<dynamic_completion_cli>("dynamic_completion_cli test binary ", 4))
           | std::views::transform(&cli::completion<>::value)
           | std::ranges::to<std::vector>();
    CHECK(std::ranges::contains(v, "*.bin"sv));
  }
  SUBCASE("archive type")
  {
    auto v = completion_values(
                 complete<dynamic_completion_cli>("dynamic_completion_cli test archive ", 4))
           | std::views::transform(&cli::completion<>::value)
           | std::ranges::to<std::vector>();
    CHECK(std::ranges::contains(v, "*.zip"sv));
  }
}

TEST_CASE("reflex::cli:bash source keeps executable directory in PATH")
{
  testutils::set_env("_REFLEX_COMPLETE", "bash_source", true);

  int         rc     = -1;
  const char* argv[] = {"/tmp/reflex/bin/reflex-cli", "--help"};
  auto [out, err] =
      testutils::capture_out_err([&] { rc = cli::run(dynamic_completion_cli{}, 2, argv); });

  testutils::unset_env("_REFLEX_COMPLETE");

  CHECK_EQ(rc, 0);
  CHECK(err.empty());
  CHECK(out.find("export PATH=\"/tmp/reflex/bin:$PATH\"") != std::string::npos);
  CHECK(
      out.find("complete -o nosort -F _reflex_cli_completion \"reflex-cli\"") != std::string::npos);
}

TEST_CASE("reflex::cli:zsh source keeps executable directory in PATH")
{
  testutils::set_env("_REFLEX_COMPLETE", "zsh_source", true);

  int         rc     = -1;
  const char* argv[] = {"/tmp/reflex/bin/reflex-cli", "--help"};
  auto [out, err] =
      testutils::capture_out_err([&] { rc = cli::run(dynamic_completion_cli{}, 2, argv); });

  testutils::unset_env("_REFLEX_COMPLETE");

  CHECK_EQ(rc, 0);
  CHECK(err.empty());
  CHECK(out.find("export PATH=\"/tmp/reflex/bin:$PATH\"") != std::string::npos);
  CHECK(
      out.find("_REFLEX_COMPLETE=zsh_complete \"/tmp/reflex/bin/reflex-cli\"")
      != std::string::npos);
  CHECK(out.find("compdef _reflex_cli_completion \"reflex-cli\"") != std::string::npos);
}

#include <bitflag_cli.hpp>

TEST_CASE("reflex::cli:bitflag enum completion operators")
{
  SUBCASE("zsh mode completes += expression")
  {
    auto values = completion_values_from_stream(
        complete_with_mode<bitflag_enum_cli>("zsh_complete", "bitflag_enum_cli set test+=SOS_", 3));

    CHECK(std::ranges::contains(values, std::string{"test+=HELLO"}));
    CHECK(std::ranges::contains(values, std::string{"test+=WORLD"}));
  }

  SUBCASE("zsh mode completes -= expression")
  {
    auto values = completion_values_from_stream(
        complete_with_mode<bitflag_enum_cli>("zsh_complete", "bitflag_enum_cli set test-=HE", 3));

    CHECK(std::ranges::contains(values, std::string{"test-=HELLO"}));
  }
}
