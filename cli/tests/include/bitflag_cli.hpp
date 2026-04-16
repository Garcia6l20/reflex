#pragma once

#ifndef MODULE_MODE
#include <reflex/cli.hpp>
#else
import reflex.cli;
import std;
#endif

using namespace reflex;

struct[[= cli::command{"Bitflag completion reproducer."}]] bitflag_enum_cli
{
  struct[[= cli::command{"Set config field."}]] set_command
  {
    auto gpio_completer(std::string_view current) const
    {
      constexpr std::array flags{"HELLO", "WORLD"};

      auto const op_pos = [&] {
        constexpr auto ops = std::array<std::string_view, 6>{
            "=",
            "+=",
            "-=",
            "|=",
            "&=",
            "^=",
        };
        for (const auto& op : ops)
        {
          if(auto pos = current.rfind(op); pos != std::string_view::npos)
          {
            return std::pair{pos, std::string_view{op}};
          }
        }
        return std::pair{std::string_view::npos, std::string_view{}};
      }();

      std::string_view prefix;
      std::string_view fragment = current;
      if(op_pos.first != std::string_view::npos)
      {
        prefix   = current.substr(0, op_pos.first + op_pos.second.size());
        fragment = current.substr(op_pos.first + op_pos.second.size());
      }

      std::vector<cli::completion<>> out;
      for(auto const* flag : flags)
      {
        std::string_view const flag_sv{flag};
        if(fragment.empty() or flag_sv.starts_with(fragment))
        {
          out.push_back(
              {.type        = cli::completion_type::plain,
               .value       = std::string(prefix) + std::string(flag_sv),
               .description = "bitflag"});
        }
      }

      return std::make_tuple(true, out);
    }

    [[= cli::argument{"Bitflag expression."}, = cli::complete{^^gpio_completer}]] std::string value;
    int operator()() const
    {
      return 0;
    }
  } set{};
};
