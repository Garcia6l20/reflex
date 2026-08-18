#include "moc-pair.hpp"

#include <reflex/qt/moc/export.hpp>

#include <cstdlib>
#include <print>
#include <string>
#include <string_view>

REFLEX_QT_MODULE(pair_types, m)
{
  m.expose<twin>();
}

int main(int argc, char** argv)
{
  reflex::qt::moc::options opts;
  std::string              output;

  for(int i = 1; i < argc; ++i)
  {
    const std::string_view arg{argv[i]};
    if(arg == "-I" and i + 1 < argc)
    {
      opts.include_roots.emplace_back(argv[++i]);
    }
    else if(arg.starts_with("-I"))
    {
      opts.include_roots.emplace_back(arg.substr(2));
    }
    else
    {
      output = arg;
    }
  }

  if(output.empty())
  {
    std::println("{}", reflex::qt::moc::format_metatypes<pair_types>(opts));
    return EXIT_SUCCESS;
  }
  return reflex::qt::moc::write_metatypes<pair_types>(output, opts);
}
