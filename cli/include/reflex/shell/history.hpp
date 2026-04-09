#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <deque>

#include <reflex/cli.hpp>
#endif

#include <reflex/shell/codes.hpp>

REFLEX_EXPORT namespace reflex::shell
{
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
}
