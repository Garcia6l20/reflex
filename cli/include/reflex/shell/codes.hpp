#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <format>
#include <iostream>
#endif

REFLEX_EXPORT namespace reflex::shell
{
  namespace codes
  {
  constexpr auto esc_seq = '\x1b';
  constexpr auto bs      = '\b';
  constexpr auto del     = '\x7f';
  constexpr auto eot     = '\x04';
  constexpr auto tab     = '\t';
  constexpr auto bel     = '\a';
  constexpr auto lf      = '\n';
  constexpr auto cr      = '\r';
  constexpr auto ctrl_w  = '\x17';
  } // namespace codes

  namespace sequences
  {
  constexpr auto csi = "\x1b[";

  constexpr auto cursor_up(std::size_t n)
  {
    return std::format("\x1b[{}A", n);
  }
  constexpr auto cursor_down(std::size_t n)
  {
    return std::format("\x1b[{}B", n);
  }
  constexpr auto cursor_right(std::size_t n)
  {
    return std::format("\x1b[{}C", n);
  }
  constexpr auto cursor_left(std::size_t n)
  {
    return std::format("\x1b[{}D", n);
  }
  constexpr auto cursor_to_column(std::size_t n)
  {
    return std::format("\x1b[{}G", n);
  }

  constexpr auto arrow_up    = "\x1b[A";
  constexpr auto arrow_down  = "\x1b[B";
  constexpr auto arrow_right = "\x1b[C";
  constexpr auto arrow_left  = "\x1b[D";

  constexpr auto delete_char  = "\x1b[P";
  constexpr auto insert_char  = "\x1b[@";
  constexpr auto clear_line   = "\x1b[K";
  constexpr auto clear_screen = "\x1b[2J\x1b[H";
  constexpr auto clear_to_eol = "\x1b[K";
  constexpr auto clear_to_bol = "\x1b[1K";
  constexpr auto cursor_home  = "\x1b[H";
  constexpr auto cursor_end   = "\x1b[F";

  } // namespace sequences

  namespace detail
  {
  inline void move_cursor_left(std::size_t count)
  {
    if(count > 0)
    {
      std::cout << sequences::cursor_left(count);
    }
  }

  inline void move_cursor_right(std::size_t count)
  {
    if(count > 0)
    {
      std::cout << sequences::cursor_right(count);
    }
  }

  inline void move_cursor_to_input_start(std::size_t prompt_length)
  {
    std::cout << sequences::cursor_to_column(prompt_length + 1);
  }
  } // namespace detail
}
