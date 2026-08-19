#pragma once

#include <reflex/qt/moc/json.hpp>
#include <reflex/qt/moc/module.hpp>
#include <reflex/serde/json.hpp>

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <print>
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
 * compile line that builds that file.
 *
 * @ref include_roots names the include directories a consumer will pass, and is
 * what makes the spelling short and relocatable: `include_roots = {"include"}`
 * turns `/src/app/include/app/thing.hpp` into `app/thing.hpp`. With no root
 * matching, the spelling is the absolute path of the header.
 *
 * @ref compile_dir is the directory the compiler ran in. A build that compiles
 * with relative paths - ninja does - leaves relative paths in
 * `std::source_location::file_name()`, and the binary carries nothing else, so
 * that directory cannot be recovered from the binary and is an input. It is
 * required exactly when a recorded path is relative; a build that compiles with
 * absolute paths needs none. A header that does not exist once resolved is an
 * error rather than a guess either way.
 */
struct options
{
  std::vector<std::filesystem::path> include_roots;
  std::filesystem::path              compile_dir{};
};

namespace detail
{
/** @brief @p file as the compiler's path, rooted at @ref options::compile_dir */
inline auto resolved_source(std::filesystem::path const& file, options const& opts)
    -> std::filesystem::path
{
  if(file.is_absolute() or opts.compile_dir.empty())
  {
    return file;
  }
  return opts.compile_dir / file;
}

/** @brief @p file as an angled include, relative to the shortest matching root */
inline auto include_spelling_of(std::filesystem::path const& file, options const& opts)
    -> std::string
{
  std::error_code error;
  const auto      rooted   = resolved_source(file, opts);
  const auto      absolute = std::filesystem::weakly_canonical(rooted, error);
  const auto&     source   = error ? rooted : absolute;
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

/** @brief a header the options do not pin to a file, and why */
struct unresolved_source
{
  std::string path;
  std::string reason;
};

/** @brief the headers @p Module exposes that @p opts does not pin to a file
 *
 * Two ways to be unpinned: the compiler recorded a relative path and
 * @ref options::compile_dir is empty, so only the process' working directory
 * could complete it; or the completed path names nothing. Either way the
 * document would carry a spelling no consumer can include.
 */
template <typename Module> auto unresolved_sources(options const& opts = {})
    -> std::vector<unresolved_source>
{
  std::vector<unresolved_source> missing;

  const auto seen = [&missing](std::string_view path)
  {
    return std::ranges::any_of(missing, [path](auto const& m) { return m.path == path; });
  };

  template for(constexpr auto t : exposed_types<Module>)
  {
    constexpr auto source =
        std::define_static_string(std::string_view{source_location_of(t).file_name()});

    const std::filesystem::path recorded{source};
    if(not recorded.is_absolute() and opts.compile_dir.empty())
    {
      if(const auto printed = recorded.generic_string(); not seen(printed))
      {
        missing.push_back({printed, "the compiler recorded a relative path, pass -C <the "
                                    "directory the compiler ran in>"});
      }
      continue;
    }

    const auto resolved = detail::resolved_source(recorded, opts);
    if(const auto printed = resolved.generic_string();
       not std::filesystem::exists(resolved) and not seen(printed))
    {
      missing.push_back({printed, "no such file"});
    }
  }
  return missing;
}

namespace detail
{
/** @brief reports every unresolved header on stderr; true when there was one */
template <typename Module> auto report_unresolved(options const& opts) -> bool
{
  const auto missing = unresolved_sources<Module>(opts);
  for(auto const& source : missing)
  {
    std::println(stderr, "cannot resolve {}: {}", source.path, source.reason);
  }
  return not missing.empty();
}
} // namespace detail

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
 * Nothing is opened until every exposed header resolves, so a failure leaves no
 * truncated document behind. Every failure names what failed on stderr.
 *
 * @return `EXIT_SUCCESS`, or `EXIT_FAILURE` when a header did not resolve or
 *         @p output could not be written.
 */
template <typename Module>
auto write_metatypes(std::filesystem::path const& output, options const& opts = {}) -> int
{
  if(detail::report_unresolved<Module>(opts))
  {
    return EXIT_FAILURE;
  }

  std::ofstream stream(output);
  if(not stream)
  {
    std::println(stderr, "cannot write {}: {}", output.string(), std::strerror(errno));
    return EXIT_FAILURE;
  }

  stream << format_metatypes<Module>(opts) << '\n';
  stream.close();
  if(not stream)
  {
    std::println(stderr, "cannot write {}: {}", output.string(), std::strerror(errno));
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

/** @brief the exporter program, driven by the command line the build passes it
 *
 * `reflex_build.qt.add_metatypes` runs the exporter as
 * `<program> <output> -I <root> ...`, so an exporter's `main` is one line:
 *
 * ```cpp
 * int main(int argc, char** argv)
 * {
 *   return reflex::qt::moc::export_main<app_types>(argc, argv);
 * }
 * ```
 *
 * ```
 * usage: <program> <output|-> [-I <include-root>]... [-C <compile-dir>]
 * ```
 *
 * The one positional is the output path, or `-` for stdout. `-I` roots become
 * @ref options::include_roots, `-C` becomes @ref options::compile_dir. Both
 * take `-I<dir>` and `-I <dir>`. A missing directory, a second positional, a
 * second `-C` and any other `-`-leading argument are each an error naming
 * themselves, never an output path.
 *
 * @return `EXIT_SUCCESS`, or `EXIT_FAILURE` after a message on stderr.
 */
template <typename Module> auto export_main(int argc, char** argv) -> int
{
  const std::string_view program = argc > 0 ? argv[0] : "exporter";
  const auto             usage   = [program]
  {
    std::println(stderr, "usage: {} <output|-> [-I <include-root>]... [-C <compile-dir>]", program);
    return EXIT_FAILURE;
  };

  options     opts;
  std::string output;

  for(int i = 1; i < argc; ++i)
  {
    const std::string_view arg{argv[i]};
    std::string_view       value;

    if(arg == "-I" or arg == "-C")
    {
      if(i + 1 == argc)
      {
        std::println(stderr, "{}: {} needs a directory", program, arg);
        return usage();
      }
      value = argv[++i];
    }
    else if(arg.starts_with("-I") or arg.starts_with("-C"))
    {
      value = arg.substr(2);
    }
    else if(arg.size() > 1 and arg.starts_with('-'))
    {
      std::println(stderr, "{}: unknown option {}", program, arg);
      return usage();
    }
    else
    {
      if(not output.empty())
      {
        std::println(stderr, "{}: more than one output path: {} then {}", program, output, arg);
        return usage();
      }
      output = arg;
      continue;
    }

    if(value.empty())
    {
      std::println(stderr, "{}: {} needs a directory", program, arg.substr(0, 2));
      return usage();
    }
    if(arg.starts_with("-I"))
    {
      opts.include_roots.emplace_back(value);
    }
    else if(not opts.compile_dir.empty())
    {
      std::println(
          stderr, "{}: more than one -C directory: {} then {}", program,
          opts.compile_dir.string(), value);
      return usage();
    }
    else
    {
      opts.compile_dir = value;
    }
  }

  if(output.empty())
  {
    std::println(stderr, "{}: no output path", program);
    return usage();
  }
  if(output != "-")
  {
    return write_metatypes<Module>(output, opts);
  }
  if(detail::report_unresolved<Module>(opts))
  {
    return EXIT_FAILURE;
  }
  std::cout << format_metatypes<Module>(opts) << '\n';
  std::cout.flush();
  return std::cout ? EXIT_SUCCESS : EXIT_FAILURE;
}
} // namespace reflex::qt::moc
