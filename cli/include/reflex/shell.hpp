#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#include <reflex/shell/term.hpp>

REFLEX_EXPORT namespace reflex::shell
{
  template <typename Cli> class shell
  {
    static constexpr auto cli_type                  = decay(^^Cli);
    static constexpr auto default_history_page_size = 10UZ;

    std::string prompt_;
    std::size_t history_page_size_ = default_history_page_size;
    Cli&&       cli_;

  public:
    shell(Cli&& cli, std::size_t history_page_size = default_history_page_size)
        : prompt_(std::format("{}> ", identifier_of(cli_type))),
          history_page_size_(std::max(1uz, history_page_size)), cli_(std::forward<Cli>(cli))
    {}

    int run()
    {
      line_type                                 line;
      std::inplace_vector<std::string_view, 32> args{};
      term<std::decay_t<Cli>>                   reader{prompt_, history_page_size_};
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
          const auto rc = reflex::cli::detail::process_cmdline(
              cli_, identifier_of(cli_type), args.begin(), args.end());
          if(rc != 0)
          {
            std::cerr << "Error: command exited with code " << rc << "\n";
          }
        }
      }
      return 0;
    }
  };

  template <typename Cli>
  int run(Cli && cli, [[maybe_unused]] int argc, [[maybe_unused]] const char** argv)
  {
    shell<Cli> sh{std::forward<Cli>(cli)};
    return sh.run();
  }

  template <typename Cli, std::ranges::range R = std::initializer_list<std::string_view>>
  int run(Cli && cli, R && args)
  {
    auto argv = args
              | std::views::transform([](std::string_view arg) { return arg.data(); })
              | std::ranges::to<std::vector>();
    return reflex::shell::run(std::forward<Cli>(cli), argv.size(), argv.data());
  }

} // namespace reflex::shell
