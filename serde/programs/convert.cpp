import reflex.cli;

import std;

import reflex.serde.json;
import reflex.serde.bson;

using namespace reflex;

auto input_format_completer(std::string_view current)
{
  using namespace std::string_view_literals;
  static constexpr auto formats =
      define_static_array(reflex::serde::serializers() | std::views::transform([](auto entry) {
                            return constant_string{identifier_of(entry)};
                          }));
  return formats
       | std::views::filter(
             [current](std::string_view b) { return current.empty() or b.starts_with(current); })
       | std::views::transform([](std::string_view b) {
           return cli::completion<>{
               .value = std::string(b), .description = "Available input formats"};
         });
}

auto output_format_completer(std::string_view current)
{
  using namespace std::string_view_literals;
  static constexpr auto formats =
      define_static_array(reflex::serde::serializers() | std::views::transform([](auto entry) {
                            return constant_string{identifier_of(entry)};
                          }));
  return formats
       | std::views::filter(
             [current](std::string_view b) { return current.empty() or b.starts_with(current); })
       | std::views::transform([](std::string_view b) {
           return cli::completion<>{
               .value = std::string(b), .description = "Available output formats"};
         });
}

struct[[= cli::command("Convert file formats")]] convert_command
{
  [[= cli::option{"-v/--verbose", "Increase verbosity level"}.counter()]] int verbose = 0;

  [[
    = cli::option("-if/--input-format", "Force input format."),
    = cli::complete{^^input_format_completer}
  ]] std::string_view input_format{};

  [[
    = cli::option("-of/--output-format", "Force output format."),
    = cli::complete{^^output_format_completer}
  ]] std::string_view output_format{};

  [[= cli::argument("Input file path."), = cli::completers::path{}]] std::string  input_file{};
  [[= cli::argument("Output file path."), = cli::completers::path{}]] std::string output_file{};

  int operator()()
  {
    std::println("Converting '{}' to '{}'", input_file, output_file);

    std::filesystem::path input_path{input_file};
    std::filesystem::path output_path{output_file};

    if(input_format.empty())
    {
      input_format = input_path.extension().string();
      input_format.remove_prefix(1);
    }
    if(output_format.empty())
    {
      output_format = output_path.extension().string();
      output_format.remove_prefix(1);
    }

    std::ifstream input_stream{input_file, std::ios::binary};
    auto          in = std::ranges::subrange{
        std::istreambuf_iterator<char>(input_stream), std::istreambuf_iterator<char>()};

    serde::with_deserializer(input_format, in, [&]([[maybe_unused]] auto&& de) {
      std::ofstream output_stream{output_file, std::ios::binary};
      auto          out = std::ostreambuf_iterator<char>(output_stream);

      serde::with_serializer(output_format, out, [&]([[maybe_unused]] auto&& ser) {
        if(verbose > 0)
          std::println("Using deserializer '{}'", display_string_of(decay(^^decltype(de))));

        if(verbose > 0)
          std::println("Using serializer '{}'", display_string_of(decay(^^decltype(ser))));

        const auto value = serde::deserialize(de);

        if(verbose > 0)
          std::println("Deserialized value: {}", value);

        serde::serialize(ser, value);
      });
    });

    return 0;
  }
};

int main(int argc, const char** argv)
{
  return cli::run<convert_command>(argc, argv);
}
