#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#if __has_include(<termios.h>) && __has_include(<unistd.h>)
#include <termios.h>
#include <unistd.h>
#else
#error "Terminal handling requires <termios.h> and <unistd.h>"
#endif

#include <reflex/cli.hpp>
#endif

#include <reflex/shell/completion_session.hpp>
#include <reflex/shell/history.hpp>

REFLEX_EXPORT namespace reflex::shell
{
  template <typename Cli> class term
  {
    static constexpr std::size_t default_history_page_size = 10;

    std::string_view        prompt_;
    termios                 original_{};
    bool                    raw_enabled_       = false;
    std::size_t             history_page_size_ = default_history_page_size;
    history<64>             history_{};
    completion_session<Cli> completion_{};

    static void insert_char(line_type& line, std::size_t& cursor, char ch)
    {
      if(cursor < line.size())
      {
        line.insert(line.begin() + static_cast<std::ptrdiff_t>(cursor), ch);
      }
      else
      {
        line.push_back(ch);
      }
      ++cursor;
    }

    static bool erase_before_cursor(line_type& line, std::size_t& cursor)
    {
      if(cursor == 0)
      {
        return false;
      }
      --cursor;
      line.erase(cursor, 1);
      return true;
    }

    static bool erase_at_cursor(line_type& line, std::size_t cursor)
    {
      if(cursor >= line.size())
      {
        return false;
      }
      line.erase(cursor, 1);
      return true;
    }

    static void redraw_after_edit(line_type const& line, std::size_t cursor)
    {
      if(cursor < line.size())
      {
        auto tail = std::string_view{line}.substr(cursor);
        std::cout << tail;
        detail::move_cursor_left(line.size() - cursor);
      }
      else
      {
        std::cout << sequences::clear_to_eol;
      }
      std::cout.flush();
    }

    // Redraws from cursor to end of line and clears any stale characters beyond the new end.
    static void redraw_line_from(line_type const& line, std::size_t cursor)
    {
      auto tail = std::string_view{line}.substr(cursor);
      std::cout << tail << sequences::clear_to_eol;
      detail::move_cursor_left(line.size() - cursor);
      std::cout.flush();
    }

    static std::size_t erase_word_before_cursor(line_type& line, std::size_t& cursor)
    {
      if(cursor == 0)
      {
        return 0;
      }
      auto       target = find_prev_word_start(line, cursor);
      const auto erased = cursor - target;
      line.erase(target, erased);
      cursor = target;
      return erased;
    }

    static std::size_t erase_word_at_cursor(line_type& line, std::size_t cursor)
    {
      if(cursor >= line.size())
      {
        return 0;
      }
      auto       target = find_next_word_end(line, cursor);
      const auto erased = target - cursor;
      line.erase(cursor, erased);
      return erased;
    }

    void replace_visible_line(line_type& line, std::size_t& cursor, std::string_view replacement)
    {
      detail::move_cursor_to_input_start(prompt_.size());
      line.assign(replacement.begin(), replacement.end());
      std::cout << sequences::clear_to_eol << line;
      cursor = line.size();
      std::cout.flush();
    }

    static bool is_word_char(char ch)
    {
      const auto uch = static_cast<unsigned char>(ch);
      return is_alphanum(uch) or ch == '-' or ch == '_';
    }

    static std::size_t find_prev_word_start(line_type const& line, std::size_t cursor)
    {
      auto target = cursor;
      while(target > 0 and not is_word_char(line[target - 1]))
      {
        --target;
      }
      while(target > 0 and is_word_char(line[target - 1]))
      {
        --target;
      }
      return target;
    }

    static std::size_t find_next_word_end(line_type const& line, std::size_t cursor)
    {
      auto target = cursor;
      while(target < line.size() and is_word_char(line[target]))
      {
        ++target;
      }
      while(target < line.size() and not is_word_char(line[target]))
      {
        ++target;
      }
      return target;
    }

    static void move_word_left(line_type const& line, std::size_t& cursor)
    {
      if(cursor == 0)
      {
        return;
      }

      auto target = find_prev_word_start(line, cursor);
      auto delta  = cursor - target;
      cursor      = target;
      detail::move_cursor_left(delta);
      std::cout.flush();
    }

    static void move_home([[maybe_unused]] line_type const& line, std::size_t& cursor)
    {
      if(cursor == 0)
      {
        return;
      }
      detail::move_cursor_left(cursor);
      cursor = 0;
      std::cout.flush();
    }

    static void move_end(line_type const& line, std::size_t& cursor)
    {
      if(cursor >= line.size())
      {
        return;
      }
      detail::move_cursor_right(line.size() - cursor);
      cursor = line.size();
      std::cout.flush();
    }

    static void move_word_right(line_type const& line, std::size_t& cursor)
    {
      if(cursor >= line.size())
      {
        return;
      }

      auto target = find_next_word_end(line, cursor);
      auto delta  = target - cursor;
      cursor      = target;
      detail::move_cursor_right(delta);
      std::cout.flush();
    }

    void handle_arrow(char code, line_type& line, std::size_t& cursor)
    {
      if(code == 'C')
      {
        if(cursor < line.size())
        {
          ++cursor;
          std::cout << sequences::arrow_right;
          std::cout.flush();
        }
        return;
      }

      if(code == 'D')
      {
        if(cursor > 0)
        {
          --cursor;
          std::cout << sequences::arrow_left;
          std::cout.flush();
        }
        return;
      }

      if(code == 'A')
      {
        if(auto prev = history_.previous(line))
        {
          replace_visible_line(line, cursor, *prev);
        }
        return;
      }

      if(code == 'B')
      {
        if(auto next = history_.next())
        {
          replace_visible_line(line, cursor, *next);
        }
        return;
      }
    }

    void move_history_page_up(line_type& line, std::size_t& cursor)
    {
      if(auto prev = history_.previous(line, history_page_size_))
      {
        replace_visible_line(line, cursor, *prev);
      }
    }

    void move_history_page_down(line_type& line, std::size_t& cursor)
    {
      if(auto next = history_.next(history_page_size_))
      {
        replace_visible_line(line, cursor, *next);
      }
    }

    void handle_csi_sequence(
        std::string_view params,
        char             final_char,
        line_type&       line,
        std::size_t&     cursor)
    {
      auto is_ctrl_modified = [&]() {
        return params.find(";5") != std::string_view::npos || params == "5";
      };

      if(final_char == 'A' or final_char == 'B')
      {
        handle_arrow(final_char, line, cursor);
        return;
      }

      if(final_char == 'H')
      {
        move_home(line, cursor);
        return;
      }

      if(final_char == 'F')
      {
        move_end(line, cursor);
        return;
      }

      if(final_char == 'C' or final_char == 'D')
      {
        if(is_ctrl_modified())
        {
          if(final_char == 'C')
          {
            move_word_right(line, cursor);
          }
          else
          {
            move_word_left(line, cursor);
          }
          return;
        }

        handle_arrow(final_char, line, cursor);
        return;
      }

      if(final_char == '~' and params == "3")
      {
        if(erase_at_cursor(line, cursor))
        {
          std::cout << sequences::delete_char;
          redraw_after_edit(line, cursor);
        }
        return;
      }

      if(final_char == '~' and params == "3;5")
      {
        if(erase_word_at_cursor(line, cursor))
        {
          redraw_line_from(line, cursor);
        }
        return;
      }

      if(final_char == '~' and params == "5")
      {
        move_history_page_up(line, cursor);
        return;
      }

      if(final_char == '~' and params == "6")
      {
        move_history_page_down(line, cursor);
        return;
      }

#ifdef DEBUG
      std::println(
          std::cerr, "\n[term] unhandled CSI: params='{}' final='{}'\n", std::string{params},
          final_char);
#endif
    }

  public:
    term(std::string_view prompt, std::size_t history_page_size = default_history_page_size)
        : prompt_(prompt), history_page_size_(std::max(1uz, history_page_size)),
          completion_(prompt.size())
    {
      if(::isatty(STDIN_FILENO) == 0)
      {
        return;
      }

      if(::tcgetattr(STDIN_FILENO, &original_) != 0)
      {
        return;
      }

      auto raw = original_;
      raw.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO));
      raw.c_cc[VMIN]  = 1;
      raw.c_cc[VTIME] = 0;

      if(::tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != 0)
      {
        throw runtime_error("Failed to set terminal to raw mode");
      }

      raw_enabled_ = true;
    }

    ~term()
    {
      if(raw_enabled_)
      {
        ::tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_);
      }
    }

    term(term const&)            = delete;
    term& operator=(term const&) = delete;

    bool read_line(line_type& line)
    {
      line.clear();
      history_.reset_navigation();
      completion_.reset();

      std::size_t cursor = 0;
      while(true)
      {
        char       ch = 0;
        const auto n  = ::read(STDIN_FILENO, &ch, 1);
        if(n <= 0)
        {
          return false;
        }

        if(ch != codes::tab)
        {
          completion_.reset();
        }

        if(ch == codes::lf || ch == codes::cr)
        {
          std::cout << codes::lf;
          std::cout.flush();
          history_.push(line);
          return true;
        }

        if(ch == codes::eot)
        {
          // Ctrl-D behaves like EOF if line is empty.
          if(line.empty())
          {
            return false;
          }
          continue;
        }

        if(ch == codes::ctrl_w)
        {
          if(auto n = erase_word_before_cursor(line, cursor))
          {
            detail::move_cursor_left(n);
            redraw_line_from(line, cursor);
          }
          continue;
        }

        if(ch == codes::bs or ch == codes::del)
        {
          if(erase_before_cursor(line, cursor))
          {
            std::cout << sequences::arrow_left << sequences::delete_char;
            redraw_after_edit(line, cursor);
          }
          continue;
        }

        if(ch == codes::esc_seq)
        {
          char seq1 = 0;
          if(::read(STDIN_FILENO, &seq1, 1) <= 0)
          {
            continue;
          }

          if(seq1 == codes::del)
          {
            // ESC + DEL = Ctrl+Backspace / Alt+Backspace => erase previous word.
            if(auto n = erase_word_before_cursor(line, cursor))
            {
              detail::move_cursor_left(n);
              redraw_line_from(line, cursor);
            }
            continue;
          }

          if(seq1 == 'd')
          {
            // ESC + d = Alt+D / Ctrl+Delete => erase next word.
            if(erase_word_at_cursor(line, cursor))
            {
              redraw_line_from(line, cursor);
            }
            continue;
          }

          if(seq1 != '[')
          {
#ifdef DEBUG
            std::println(
                std::cerr, "\n[term] unhandled escape: seq=0x{:02x}\n",
                static_cast<unsigned char>(seq1));
#endif
            continue;
          }

          {
            std::inplace_vector<char, 16> params_buf{};
            char                          final_char = 0;

            while(true)
            {
              char byte = 0;
              if(::read(STDIN_FILENO, &byte, 1) <= 0)
              {
                break;
              }

              auto              uch           = static_cast<unsigned char>(byte);
              static const auto is_final_byte = [](unsigned char c) {
                return is_in_range(c, 0x40, 0x7E);
              };
              if(is_final_byte(uch))
              {
                final_char = byte;
                break;
              }

              params_buf.push_back(byte);
            }

            if(final_char != 0)
            {
              handle_csi_sequence(std::string_view{params_buf}, final_char, line, cursor);
            }
          }
          continue;
        }

        if(ch == codes::tab)
        {
          completion_.handle_tab(line, cursor);
          continue;
        }

        if(is_print(static_cast<unsigned char>(ch)))
        {
          insert_char(line, cursor, ch);
          std::cout.put(ch);
          redraw_after_edit(line, cursor);
          continue;
        }

#ifdef DEBUG
        std::println(
            std::cerr, "\n[term] unhandled byte: 0x{:02x}\n", static_cast<unsigned char>(ch));
#endif
      }
    }
  };
} // namespace reflex::shell
