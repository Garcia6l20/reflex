module;

#if __has_include(<termios.h>) && __has_include(<unistd.h>)
#include <termios.h>
#include <unistd.h>
#else
#error "Terminal handling requires <termios.h> and <unistd.h>"
#endif

module reflex.shell:term;

import std;
import reflex.core;
import reflex.cli;

namespace reflex::shell
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

template <std::size_t N> class history
{
  std::deque<std::string>    entries_{};
  std::optional<std::string> draft_line_{};
  std::size_t                cursor_ = 0;

  std::optional<std::string_view> draft_or_empty() const
  {
    return draft_line_.has_value() ? std::optional{std::string_view{*draft_line_}}
                                   : std::optional{std::string_view{}};
  }

public:
  void push(std::string_view line)
  {
    if(line.empty())
    {
      reset_navigation();
      return;
    }

    if(!entries_.empty() && entries_.back() == line)
    {
      reset_navigation();
      return;
    }

    if(entries_.size() == N)
    {
      entries_.pop_front();
    }
    entries_.emplace_back(line);
    reset_navigation();
  }

  void reset_navigation()
  {
    cursor_ = entries_.size();
    draft_line_.reset();
  }

  std::optional<std::string_view> previous(std::string_view current_line, std::size_t steps = 1)
  {
    if(entries_.empty())
    {
      return std::nullopt;
    }

    if(cursor_ == entries_.size())
    {
      draft_line_ = std::string{current_line};
    }

    cursor_ = cursor_ > steps ? cursor_ - steps : 0;

    return std::string_view{entries_[cursor_]};
  }

  std::optional<std::string_view> next(std::size_t steps = 1)
  {
    if(entries_.empty())
    {
      return std::nullopt;
    }

    if(cursor_ >= entries_.size())
    {
      return draft_or_empty();
    }

    cursor_ = std::min(cursor_ + steps, entries_.size());
    if(cursor_ == entries_.size())
    {
      if(draft_line_.has_value() and not draft_line_->empty())
      {
        return std::string_view{*draft_line_};
      }
      else if(not entries_.empty())
      {
        return std::string_view{entries_.back()};
      }
      else
      {
        return std::nullopt;
      }
    }

    return std::string_view{entries_[cursor_]};
  }
};

template <typename Cli> class completion_session
{
  std::size_t                    prompt_length_ = 0;
  cli::detail::completion_vector completions_{};
  bool                           active_     = false;
  std::size_t                    next_index_ = 0;
  std::string                    line_prefix_{};
  std::string                    line_suffix_{};
  std::size_t                    token_start_ = 0;
  std::size_t                    token_end_   = 0;

  static std::size_t token_start_at(std::string const& line, std::size_t cursor)
  {
    auto pos = cursor;
    while(pos > 0 and not is_space(static_cast<unsigned char>(line[pos - 1])))
    {
      --pos;
    }
    return pos;
  }

  static void replace_token(
      std::size_t      prompt_length,
      std::string&     line,
      std::size_t&     cursor,
      std::size_t      token_start,
      std::size_t      token_end,
      std::string_view replacement)
  {
    detail::move_cursor_to_input_start(prompt_length);
    line.replace(token_start, token_end - token_start, replacement);
    std::cout << sequences::clear_to_eol << line;

    cursor = token_start + replacement.size();
    detail::move_cursor_left(line.size() - cursor);
    std::cout.flush();
  }

  bool can_cycle(std::string const& line) const
  {
    if(not active_)
    {
      return false;
    }
    if(completions_.empty())
    {
      return false;
    }
    if(line.size() < line_prefix_.size() + line_suffix_.size())
    {
      return false;
    }
    return std::string_view{line}.starts_with(line_prefix_)
       and std::string_view{line}.ends_with(line_suffix_);
  }

  void apply_candidate(std::string& line, std::size_t& cursor, std::string_view value)
  {
    replace_token(prompt_length_, line, cursor, token_start_, token_end_, value);
    token_end_ = token_start_ + value.size();
  }

public:
  completion_session(std::size_t prompt_length) : prompt_length_(prompt_length)
  {}

  void reset()
  {
    active_     = false;
    next_index_ = 0;
    line_prefix_.clear();
    line_suffix_.clear();
    token_start_ = 0;
    token_end_   = 0;
    completions_.clear();
  }

  void handle_tab(std::string& line, std::size_t& cursor)
  {
    if(can_cycle(line))
    {
      const auto& candidate = completions_[next_index_];
      apply_candidate(line, cursor, candidate.value);
      next_index_ = (next_index_ + 1) % completions_.size();
      return;
    }

    reset();

    token_start_ = token_start_at(line, cursor);
    token_end_   = cursor;
    line_prefix_ = line.substr(0, token_start_);
    line_suffix_ = line.substr(cursor);

    const auto current = std::string_view{line}.substr(token_start_, token_end_ - token_start_);

    std::inplace_vector<std::string_view, 32> args{};
    if(!cli::detail::tokenize(line_prefix_, std::back_inserter(args)))
    {
      std::cout << codes::bel;
      std::cout.flush();
      return;
    }

    completions_ = cli::detail::complete_for<^^Cli>(args, current);
    if(completions_.empty())
    {
      std::cout << codes::bel;
      std::cout.flush();
      return;
    }

    if(completions_.size() == 1)
    {
      std::string insert = std::string{completions_.front().value};
      if(line_suffix_.empty() or is_space(static_cast<unsigned char>(line_suffix_.front())))
      {
        insert.push_back(' ');
      }
      apply_candidate(line, cursor, insert);
      reset();
      return;
    }

    active_ = true;
    apply_candidate(line, cursor, completions_.front().value);
    next_index_ = 1;
  }
};

template <typename Cli> class term_reader
{
  static constexpr std::size_t default_history_page_size = 10;

  std::string_view        prompt_;
  termios                 original_{};
  bool                    raw_enabled_       = false;
  std::size_t             history_page_size_ = default_history_page_size;
  history<64>             history_{};
  completion_session<Cli> completion_{};

  static void insert_char(std::string& line, std::size_t& cursor, char ch)
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

  static bool erase_before_cursor(std::string& line, std::size_t& cursor)
  {
    if(cursor == 0)
    {
      return false;
    }
    --cursor;
    line.erase(cursor, 1);
    return true;
  }

  static bool erase_at_cursor(std::string& line, std::size_t cursor)
  {
    if(cursor >= line.size())
    {
      return false;
    }
    line.erase(cursor, 1);
    return true;
  }

  static void redraw_after_edit(std::string const& line, std::size_t cursor)
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
  static void redraw_line_from(std::string const& line, std::size_t cursor)
  {
    auto tail = std::string_view{line}.substr(cursor);
    std::cout << tail << sequences::clear_to_eol;
    detail::move_cursor_left(line.size() - cursor);
    std::cout.flush();
  }

  static std::size_t erase_word_before_cursor(std::string& line, std::size_t& cursor)
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

  static std::size_t erase_word_at_cursor(std::string& line, std::size_t cursor)
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

  void replace_visible_line(std::string& line, std::size_t& cursor, std::string_view replacement)
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

  static std::size_t find_prev_word_start(std::string const& line, std::size_t cursor)
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

  static std::size_t find_next_word_end(std::string const& line, std::size_t cursor)
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

  static void move_word_left(std::string const& line, std::size_t& cursor)
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

  static void move_home([[maybe_unused]] std::string const& line, std::size_t& cursor)
  {
    if(cursor == 0)
    {
      return;
    }
    detail::move_cursor_left(cursor);
    cursor = 0;
    std::cout.flush();
  }

  static void move_end(std::string const& line, std::size_t& cursor)
  {
    if(cursor >= line.size())
    {
      return;
    }
    detail::move_cursor_right(line.size() - cursor);
    cursor = line.size();
    std::cout.flush();
  }

  static void move_word_right(std::string const& line, std::size_t& cursor)
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

  void handle_arrow(char code, std::string& line, std::size_t& cursor)
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

  void move_history_page_up(std::string& line, std::size_t& cursor)
  {
    if(auto prev = history_.previous(line, history_page_size_))
    {
      replace_visible_line(line, cursor, *prev);
    }
  }

  void move_history_page_down(std::string& line, std::size_t& cursor)
  {
    if(auto next = history_.next(history_page_size_))
    {
      replace_visible_line(line, cursor, *next);
    }
  }

  void handle_csi_sequence(
      std::string_view params,
      char             final_char,
      std::string&     line,
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
  term_reader(std::string_view prompt, std::size_t history_page_size = default_history_page_size)
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

  ~term_reader()
  {
    if(raw_enabled_)
    {
      ::tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_);
    }
  }

  term_reader(term_reader const&)            = delete;
  term_reader& operator=(term_reader const&) = delete;

  bool read_line(std::string& line)
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

            auto uch = static_cast<unsigned char>(byte);
            if(uch >= 0x40 and uch <= 0x7E)
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
