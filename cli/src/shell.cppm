export module reflex.shell;

import :term;

export import reflex.cli;

import std;

export namespace reflex::shell
{
template <typename Cli> class shell
{
  static constexpr auto cli_type = type_of(^^Cli);

  std::string prompt_;
  Cli         cli_;

public:
  shell(Cli&& cli)
      : prompt_(std::format("{}> ", identifier_of(^^Cli))), cli_(std::forward<Cli>(cli))
  {}

  int run()
  {
    std::string line;
    line.reserve(32);
    std::inplace_vector<std::string_view, 32> args{};
    term_reader<Cli>                          reader{prompt_};
    while(true)
    {
      std::cout << prompt_;
      std::cout.flush();

      if(!reader.read_line(line))
      {
        break; // EOF or error
      }
      args.clear();
      if(!cli::detail::tokenize(as_const(line), std::back_inserter(args)))
      {
        std::cerr << "Error: failed to tokenize input\n";
        continue;
      }
      if(!args.empty())
      {
        int rc = reflex::cli::run(cli_, identifier_of(^^Cli), args.begin(), args.end());
        if(rc != 0)
        {
          std::cerr << "Error: command exited with code " << rc << "\n";
        }
      }
    }
    return 0;
  }
};

template <typename Cli> int run(Cli&& cli, int argc, const char** argv)
{
  shell<Cli> sh{std::forward<Cli>(cli)};
  return sh.run();
}

template <typename Cli, std::ranges::range R = std::initializer_list<std::string_view>>
int run(Cli&& cli, R&& args)
{
  auto argv = args
            | std::views::transform([](std::string_view arg) { return arg.data(); })
            | std::ranges::to<std::vector>();
  return cli::run(std::forward<Cli>(cli), argv.size(), argv.data());
}

} // namespace reflex::shell
