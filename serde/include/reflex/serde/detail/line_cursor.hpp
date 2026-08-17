#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <cstddef>
#include <format>
#include <iterator>
#include <ranges>
#include <stdexcept>
#include <string>
#include <type_traits>
#endif

#include <reflex/serde/detail/io.hpp>

REFLEX_EXPORT namespace reflex::serde::detail
{
  // A line-oriented cursor over a subrange, with pushback. Bytes and line breaks
  // only; nothing here knows what indentation is for. The explicit object
  // parameters are how a throw reaches `Self::format_name`.
  template <std::input_iterator InputIt>
  class line_cursor : public subrange_deserializer<InputIt>
  {
    using base = subrange_deserializer<InputIt>;

    // Pushback. A line-structured format cannot always be lexed with one byte of
    // lookahead - in YAML "- " is a sequence entry and "-1" is a number, ": "
    // ends a plain scalar and ":" does not - and a stream cursor is an input
    // iterator, so it cannot be copied and re-read to answer that. Hence a
    // buffer the parser peeks through instead.
    //
    // Grows on demand rather than being a fixed array: a lookahead is bounded by
    // a line, not by a constant. Reads pop by advancing ahead_pos_ and the
    // buffer is compacted once the consumed prefix dominates, which keeps a pop
    // amortized O(1) instead of erase-from-front's O(n).
    //
    // Bytes move out of cursor_ into here, so at_end() has to account for both.
    // It shadows the base's for exactly that reason.
    std::string ahead_;
    std::size_t ahead_pos_ = 0;

    // 0-based column of the next byte to be read. It has exactly one maintainer:
    // advance() increments it and next_line() zeroes it. Nothing else may move
    // the cursor.
    std::size_t column_ = 0;

  protected:
    using base::cursor_;

    // True when every unread byte is still in the range, so a reader may consume
    // a run of rest() in one go. A non-empty pushback buffer makes rest() a lie.
    bool nothing_buffered() const
    {
      return ahead_pos_ >= ahead_.size();
    }

    // A bulk consume of `n` bytes already scanned through rest(). The caller owns
    // two guarantees: nothing_buffered() was true, and the run holds no break.
    void skip_in_line(std::size_t n)
    {
      cursor_.advance(static_cast<std::ranges::range_difference_t<
                          typename base::range_cursor>>(n));
      column_ += n;
    }

    // Raw consume of one byte, no column bookkeeping. peek() has already put the
    // byte in the buffer, so this only ever pops.
    template <typename Self> char take_(this Self&& self)
    {
      // Nothing buffered, which is almost every call: consume straight from the
      // cursor and leave the buffer alone. Routing every byte through the
      // buffer instead cost a string append and a pop per byte of input, and
      // measured at roughly half the parser's total time.
      if(self.ahead_pos_ >= self.ahead_.size())
      {
        if(self.cursor_.empty())
        {
          throw std::runtime_error(std::format(
              "Unexpected end of {} input", std::remove_cvref_t<Self>::format_name));
        }
        const char c = *self.cursor_.begin();
        self.cursor_.advance(1);
        return c;
      }
      const char c = self.ahead_[self.ahead_pos_];
      ++self.ahead_pos_;
      // Drop the consumed prefix once it dominates, so the buffer tracks the
      // live lookahead rather than the whole document.
      if(self.ahead_pos_ >= 64 and self.ahead_pos_ * 2 >= self.ahead_.size())
      {
        self.ahead_.erase(0, self.ahead_pos_);
        self.ahead_pos_ = 0;
      }
      return c;
    }

  public:
    using base::base;

    bool at_end() const
    {
      return ahead_pos_ >= ahead_.size() and cursor_.empty();
    }

    // The byte `i` ahead of the cursor, or '\0' at end of input. Callers must
    // treat '\0' as "nothing there": a NUL in the input is not distinguishable
    // here, which is fine because a NUL is not valid in any of these formats.
    char peek_at(std::size_t i)
    {
      // The zero-lookahead case, which is almost every call: read through the
      // cursor rather than moving the byte into the buffer first. See take_().
      if(i == 0 and ahead_pos_ >= ahead_.size())
      {
        return cursor_.empty() ? '\0' : *cursor_.begin();
      }
      while(ahead_.size() - ahead_pos_ <= i)
      {
        if(cursor_.empty())
        {
          return '\0';
        }
        ahead_.push_back(*cursor_.begin());
        cursor_.advance(1);
      }
      return ahead_[ahead_pos_ + i];
    }

    template <typename Self> char peek(this Self&& self)
    {
      if(self.at_end())
      {
        throw std::runtime_error(
            std::format("Unexpected end of {} input", std::remove_cvref_t<Self>::format_name));
      }
      return self.peek_at(0);
    }

    // YAML 1.2 makes a lone '\r' ordinary content and only "\r\n" a break. That
    // needs two bytes of lookahead at every scalar byte, so this takes YAML
    // 1.1's rule instead: '\r' alone is a break too. The two differ only for a
    // lone CR inside a scalar, which no editor in use produces.
    static constexpr bool is_break(char c)
    {
      return c == '\n' or c == '\r';
    }

    bool at_line_end()
    {
      return at_end() or is_break(peek_at(0));
    }

    std::size_t column() const
    {
      return column_;
    }

    // Consumes one byte of the current line.
    //
    // Refuses a line break. Every caller has already tested at_line_end(), so a
    // break reaching here is a bug in the caller, not bad input - and a node
    // reader that runs past a line end has lost the column, which makes every
    // column after it wrong. Better to fail here than to mis-parse three lines
    // later.
    template <typename Self> char advance(this Self&& self)
    {
      const char c = self.peek();
      if(is_break(c))
      {
        throw std::runtime_error(std::format(
            "{}: internal - advance() over a line break",
            std::remove_cvref_t<Self>::format_name));
      }
      self.take_();
      ++self.column_;
      return c;
    }

    template <typename Self> void skip_to_line_end(this Self&& self)
    {
      while(not self.at_line_end())
      {
        self.advance();
      }
    }

    // The only function that may consume a line break. Consumes the rest of the
    // current line too. Returns false at end of input.
    template <typename Self> bool next_line(this Self&& self)
    {
      while(not self.at_end() and not is_break(self.peek_at(0)))
      {
        self.take_();
      }
      if(self.at_end())
      {
        return false;
      }
      const char c = self.take_();
      if(c == '\r' and not self.at_end() and self.peek_at(0) == '\n')
      {
        self.take_();
      }
      self.column_ = 0;
      return true;
    }
  };
} // namespace reflex::serde::detail
