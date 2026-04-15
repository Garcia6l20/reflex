#pragma once

#include <pipe_capture.hpp>

#include <reflex/cli.hpp>

#include <sstream>

namespace reflex::testutils
{
bool set_env(const char* name, const char* value, int overwrite);
bool unset_env(const char* name);

using namespace reflex;

template <typename Cli>
std::vector<std::string> complete(std::string_view comp_line, int comp_point)
{
  // Set the completion env vars, run, capture stdout.
  testutils::set_env("_REFLEX_COMPLETE", "zsh_complete", true);
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

  // Split output into lines.
  std::vector<std::string> results;
  std::istringstream       ss{out};
  std::string              line;
  while(std::getline(ss, line))
    if(not line.empty())
      results.push_back(line);
  return results;
}

// Extract just the "value" from completion output lines.
auto completion_values(const std::vector<std::string>& lines)
{
  std::vector<cli::completion> result;
  for(std::size_t i = 0; i < lines.size(); i += 3)
  {
    auto type = parse<cli::completion_type>(lines[i]);
    REQUIRE(type.has_value());
    result.push_back({type.value(), lines[i + 1], lines[i + 2]});
  }
  return result;
}

template <typename Cli> static auto run(auto&&... args)
{
  std::inplace_vector<std::string_view, 32> argv = {"git"};
  (argv.push_back(args), ...);
  int rc          = -1;
  auto [out, err] = testutils::capture_out_err(
      [&] { rc = cli::run(Cli{}, std::span{argv.data(), argv.size()}); });
  return std::tuple{rc, out, err};
}
} // namespace reflex::testutils
