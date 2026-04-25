#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <reflex/cli.hpp>
#endif

#include <reflex/shell/codes.hpp>
#include <reflex/shell/config.hpp>

REFLEX_EXPORT namespace reflex::shell
{
  template <typename Cli, cli::configuration Config = {}> class completion_session
  {
    Cli&                           cli_;
    std::size_t                    prompt_length_ = 0;
    cli::completion_vector<Config> completions_{};
    bool                           active_     = false;
    std::size_t                    next_index_ = 0;
    string<32>                     line_prefix_{};
    string<32>                     line_suffix_{};
    std::size_t                    token_start_ = 0;
    std::size_t                    token_end_   = 0;

    static std::size_t token_start_at(line_type const& line, std::size_t cursor)
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
        line_type&       line,
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

    bool can_cycle(line_type const& line) const
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

    void apply_candidate(line_type& line, std::size_t& cursor, std::string_view value)
    {
      replace_token(prompt_length_, line, cursor, token_start_, token_end_, value);
      token_end_ = token_start_ + value.size();
    }

  public:
    completion_session(Cli& cli, std::size_t prompt_length)
        : cli_(cli), prompt_length_(prompt_length)
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

    void handle_tab(line_type& line, std::size_t& cursor)
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

      cli::word_vector<Config> args{};
      if(!cli::detail::tokenize(line, std::back_inserter(args)))
      {
        std::cout << codes::bel;
        std::cout.flush();
        return;
      }
      if(is_space(line.back()))
      {
        args.push_back(std::string_view{});
      }
      const std::size_t comp_point = args.size() + (line.ends_with(' ') ? 1 : 0);
      bool              completed       = false;
      std::tie(completed, completions_) = cli::detail::complete_for<Config>(cli_, args, comp_point);
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
}
