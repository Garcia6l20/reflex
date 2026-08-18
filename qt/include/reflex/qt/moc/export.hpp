#pragma once

#include <reflex/qt/moc/json.hpp>
#include <reflex/qt/moc/module.hpp>
#include <reflex/serde/json.hpp>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace reflex::qt::moc
{
/** @brief how the exporter spells the header a class lives in
 *
 * `qmltyperegistrar` writes `#if __has_include(<inputFile>)` into its generated
 * registration, so `inputFile` has to resolve as an angled include from the
 * compile line that builds that file. The default is the absolute path the
 * compiler saw, which always resolves. Naming the include directories a consumer
 * will pass produces the short, relocatable spelling instead:
 * `include_roots = {"include"}` turns `/src/app/include/app/thing.hpp` into
 * `app/thing.hpp`.
 */
struct options
{
  std::vector<std::filesystem::path> include_roots;
};

namespace detail
{
/** @brief @p file as an angled include, relative to the shortest matching root */
inline auto include_spelling_of(std::filesystem::path const& file, options const& opts)
    -> std::string
{
  std::error_code error;
  const auto      absolute = std::filesystem::weakly_canonical(file, error);
  const auto&     source   = error ? file : absolute;
  std::string     best;

  for(auto const& root : opts.include_roots)
  {
    const auto canonical = std::filesystem::weakly_canonical(root, error);
    const auto relative  = source.lexically_relative(error ? root : canonical);
    if(relative.empty() or *relative.begin() == "..")
    {
      continue;
    }
    if(const auto spelling = relative.generic_string();
       best.empty() or spelling.size() < best.size())
    {
      best = spelling;
    }
  }
  return best.empty() ? source.generic_string() : best;
}
} // namespace detail

/** @brief the metatypes document describing every class @p Module exposes
 *
 * One entry per header, in the order the module body first reached it, which is
 * the shape `moc --collect-json` produces and `qmltyperegistrar` consumes.
 */
template <typename Module> auto metatypes_of(options const& opts = {}) -> std::vector<filemeta_data>
{
  std::vector<filemeta_data> files;

  template for(constexpr auto t : exposed_types<Module>)
  {
    constexpr auto source =
        std::define_static_string(std::string_view{source_location_of(t).file_name()});

    const auto input = detail::include_spelling_of(std::filesystem::path{source}, opts);
    const auto found = std::ranges::find_if(
        files, [&input](filemeta_data const& f) { return f.inputFile == input; });

    filemeta_data& file =
        found == files.end() ? files.emplace_back(filemeta_data{{}, input, output_revision})
                             : *found;
    file.classes.push_back(describe<typename[:t:]>());
  }
  return files;
}

/** @brief the metatypes document of @p Module, as JSON text */
template <typename Module> auto format_metatypes(options const& opts = {}) -> std::string
{
  std::string             out;
  serde::json::serializer ser{out};
  ser.dump(metatypes_of<Module>(opts));
  return out;
}

/** @brief writes @p Module's metatypes document to @p output
 *
 * The whole exporter, so a build declares a program whose `main` is one line:
 *
 * ```cpp
 * int main(int, char** argv) { return reflex::qt::moc::write_metatypes<app_types>(argv[1]); }
 * ```
 *
 * @return `EXIT_SUCCESS`, or `EXIT_FAILURE` when @p output could not be written.
 */
template <typename Module>
auto write_metatypes(std::filesystem::path const& output, options const& opts = {}) -> int
{
  std::ofstream stream(output);
  if(not stream)
  {
    return EXIT_FAILURE;
  }
  stream << format_metatypes<Module>(opts) << '\n';
  return stream ? EXIT_SUCCESS : EXIT_FAILURE;
}
} // namespace reflex::qt::moc
