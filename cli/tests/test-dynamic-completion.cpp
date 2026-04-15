#include <doctest/doctest.h>

#include <reflex/cli.hpp>
#include <testutils.hpp>

using namespace reflex;
using namespace testutils;

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
          cli::completion{
                          .type        = cli::completion_type::file,
                          .value       = pattern,
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
           | std::views::transform(&cli::completion::value)
           | std::ranges::to<std::vector>();
    CHECK(std::ranges::contains(v, "text"sv));
    CHECK(std::ranges::contains(v, "binary"sv));
    CHECK(std::ranges::contains(v, "archive"sv));
  }
  SUBCASE("text type")
  {
    auto v =
        completion_values(complete<dynamic_completion_cli>("dynamic_completion_cli test text ", 4))
        | std::views::transform(&cli::completion::value)
        | std::ranges::to<std::vector>();
    CHECK(std::ranges::contains(v, "*.txt"sv));
  }
  SUBCASE("bin type")
  {
    auto v = completion_values(
                 complete<dynamic_completion_cli>("dynamic_completion_cli test binary ", 4))
           | std::views::transform(&cli::completion::value)
           | std::ranges::to<std::vector>();
    CHECK(std::ranges::contains(v, "*.bin"sv));
  }
  SUBCASE("archive type")
  {
    auto v = completion_values(
                 complete<dynamic_completion_cli>("dynamic_completion_cli test archive ", 4))
           | std::views::transform(&cli::completion::value)
           | std::ranges::to<std::vector>();
    CHECK(std::ranges::contains(v, "*.zip"sv));
  }
}
