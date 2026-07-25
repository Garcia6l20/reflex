#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#include <reflex/jinja/expr.hpp>

REFLEX_EXPORT namespace reflex::jinja
{
  class environment;

  namespace detail
  {

  struct text;
  struct expression;
  struct if_block;
  struct for_block;
  struct include_block;
  struct block_block;

  using element = std::variant<text, expression, if_block, for_block, include_block, block_block>;

  struct text
  {
    std::string_view content;
  };

  struct expression
  {
    std::string_view expr;
  };

  struct if_block
  {
    std::string_view     condition;
    std::vector<element> then_children;
    std::vector<element> else_children;
  };

  struct for_block
  {
    std::vector<std::string_view> loop_vars;
    std::string_view              iterable;
    std::vector<element>          children;
  };

  // {% include "name" %} - resolved at render time through the environment.
  struct include_block
  {
    std::string_view name;
  };

  // {% block name %}...{% endblock %} - overridable region.
  struct block_block
  {
    std::string_view     name;
    std::vector<element> children;
  };

  enum class block_end_kind
  {
    elif_,
    else_,
    endif_,
    endfor_,
    endblock_,
  };

  struct block_end
  {
    block_end_kind   kind;
    std::string_view tag_content; // carries the elif condition string_view
    bool             trim_next_text_left{false};
  };

  struct children_result
  {
    std::vector<element>     children;
    std::optional<block_end> end_tag;
  };

  struct if_block_result
  {
    if_block block;
    bool     trim_next_text_left{false};
  };

  constexpr bool is_trim_char(char c)
  {
    return c == ' ' or c == '\t' or c == '\n' or c == '\r';
  }

  constexpr void trim_left(std::string_view& s)
  {
    while(!s.empty() and is_trim_char(s.front()))
    {
      s.remove_prefix(1);
    }
  }

  constexpr void trim_right(std::string_view& s)
  {
    while(!s.empty() and is_trim_char(s.back()))
    {
      s.remove_suffix(1);
    }
  }

  constexpr void trim_right_last_text(std::vector<element>& children)
  {
    if(children.empty())
    {
      return;
    }

    auto* txt = std::get_if<text>(&children.back());
    if(txt == nullptr)
    {
      return;
    }

    auto content = txt->content;
    trim_right(content);
    txt->content = content;

    if(txt->content.empty())
    {
      children.pop_back();
    }
  }

  constexpr std::string_view consume_until(std::string_view& input, std::string_view marker)
  {
    auto pos = input.find(marker);
    if(pos == std::string_view::npos)
    {
      throw std::runtime_error(std::format("Unclosed tag: missing '{}'", marker));
    }
    auto content = input.substr(0, pos);
    input.remove_prefix(pos + marker.size());
    return content;
  }

  // Unquotes the single argument of {% include "x" %} / {% extends 'x' %}.
  constexpr std::string_view parse_string_literal(std::string_view arg, std::string_view tag)
  {
    if(arg.size() < 2 or (arg.front() != '"' and arg.front() != '\'') or arg.back() != arg.front())
    {
      throw std::runtime_error(std::format("'{}' expects a quoted template name", tag));
    }

    auto name = arg.substr(1, arg.size() - 2);
    if(name.empty())
    {
      throw std::runtime_error(std::format("'{}' expects a non-empty template name", tag));
    }
    return name;
  }

  constexpr std::vector<std::string_view> parse_loop_vars(std::string_view vars_str)
  {
    std::vector<std::string_view> result;
    while(!vars_str.empty())
    {
      auto comma = vars_str.find(',');
      auto var   = trim(vars_str.substr(0, comma));
      if(!var.empty())
        result.push_back(var);
      if(comma == std::string_view::npos)
        break;
      vars_str.remove_prefix(comma + 1);
    }
    return result;
  }

  constexpr children_result
      parse_children(std::string_view& input, bool trim_next_text_left = false);

  constexpr if_block_result parse_if_block(
      std::string_view  condition,
      std::string_view& input,
      bool              trim_next_text_left = false)
  {
    if_block block;
    block.condition = condition;

    auto result = parse_children(input, trim_next_text_left);

    if(!result.end_tag)
    {
      throw std::runtime_error("Unterminated {% if %} / {% elif %} block");
    }

    block.then_children = std::move(result.children);

    switch(result.end_tag->kind)
    {
      case block_end_kind::endif_:
        // no else branch
        return {std::move(block), result.end_tag->trim_next_text_left};

      case block_end_kind::else_:
      {
        auto else_result = parse_children(input, result.end_tag->trim_next_text_left);
        if(!else_result.end_tag || else_result.end_tag->kind != block_end_kind::endif_)
        {
          throw std::runtime_error("Expected {% endif %} after {% else %}");
        }
        block.else_children = std::move(else_result.children);
        return {std::move(block), else_result.end_tag->trim_next_text_left};
      }

      case block_end_kind::elif_:
      {
        // Recursively build the elif as a nested if_block in else_children.
        auto elif_result =
            parse_if_block(result.end_tag->tag_content, input, result.end_tag->trim_next_text_left);
        block.else_children = {element{std::move(elif_result.block)}};
        return {std::move(block), elif_result.trim_next_text_left};
      }

      default:
        throw std::runtime_error("Unexpected block-end tag inside {% if %}");
    }

    return {std::move(block), false};
  }

  constexpr children_result parse_children(std::string_view& input, bool trim_next_text_left)
  {
    std::vector<element> children;

    while(!input.empty())
    {
      auto next = input.find('{');
      if(next == std::string_view::npos)
      {
        if(!input.empty())
        {
          auto content = input;
          if(trim_next_text_left)
          {
            trim_left(content);
            trim_next_text_left = false;
          }
          if(!content.empty())
          {
            children.push_back(text{content});
          }
        }
        input = {};
        break;
      }

      // Collect raw text before the tag
      if(next > 0)
      {
        auto content = input.substr(0, next);
        if(trim_next_text_left)
        {
          trim_left(content);
          trim_next_text_left = false;
        }
        if(!content.empty())
        {
          children.push_back(text{content});
        }
        input.remove_prefix(next);
      }

      if(input.size() < 2)
      {
        auto content = input;
        if(trim_next_text_left)
        {
          trim_left(content);
          trim_next_text_left = false;
        }
        if(!content.empty())
        {
          children.push_back(text{content});
        }
        input = {};
        break;
      }

      char second = input[1];

      if(second == '{')
      {
        // Expression: {{ expr }}
        input.remove_prefix(2);
        auto raw = consume_until(input, "}}");
        children.push_back(expression{trim(raw)});
      }
      else if(second == '#')
      {
        // Comment: {# ... #}
        input.remove_prefix(2);
        consume_until(input, "#}");
      }
      else if(second == '%')
      {
        // Block tag: {% ... %}
        input.remove_prefix(2);
        bool left_trim = false;
        if(!input.empty() and input.front() == '-')
        {
          left_trim = true;
          input.remove_prefix(1);
        }

        auto tag_content = consume_until(input, "%}");
        bool right_trim  = false;
        if(!tag_content.empty() and tag_content.back() == '-')
        {
          right_trim = true;
          tag_content.remove_suffix(1);
        }

        if(left_trim)
        {
          trim_right_last_text(children);
        }

        auto trimmed = trim(tag_content);

        if(trimmed == "endif")
        {
          return {
              std::move(children), block_end{block_end_kind::endif_, {}, right_trim}
          };
        }

        if(trimmed == "endfor")
        {
          return {
              std::move(children), block_end{block_end_kind::endfor_, {}, right_trim}
          };
        }

        if(trimmed == "endblock" or trimmed.starts_with("endblock "))
        {
          // {% endblock name %} carries the name back for validation against the open block
          auto name = trim(trimmed.substr(8));
          return {
              std::move(children), block_end{block_end_kind::endblock_, name, right_trim}
          };
        }

        if(trimmed == "else")
        {
          return {
              std::move(children), block_end{block_end_kind::else_, {}, right_trim}
          };
        }

        if(trimmed.starts_with("elif"))
        {
          auto cond = trim(trimmed.substr(4));
          if(cond.empty())
            throw std::runtime_error("Empty condition in {% elif %}");
          return {
              std::move(children), block_end{block_end_kind::elif_, cond, right_trim}
          };
        }

        if(trimmed.starts_with("if ") || trimmed == "if")
        {
          auto cond = trim(trimmed.substr(2));
          if(cond.empty())
            throw std::runtime_error("Empty condition in {% if %}");

          auto if_result = parse_if_block(cond, input, right_trim);
          if(if_result.trim_next_text_left)
          {
            trim_next_text_left = true;
          }
          children.push_back(element{std::move(if_result.block)});
        }
        else if(trimmed.starts_with("for "))
        {
          auto rest = trim(trimmed.substr(3));
          auto in   = rest.find(" in ");
          if(in == std::string_view::npos)
          {
            throw std::runtime_error("Malformed {% for %}: missing 'in' keyword");
          }

          auto vars_str = trim(rest.substr(0, in));
          auto iterable = trim(rest.substr(in + 4));

          for_block block;
          block.loop_vars = parse_loop_vars(vars_str);
          block.iterable  = iterable;

          if(block.loop_vars.empty())
          {
            throw std::runtime_error("Malformed {% for %}: missing loop variable(s)");
          }

          auto result    = parse_children(input, right_trim);
          block.children = std::move(result.children);

          if(!result.end_tag || result.end_tag->kind != block_end_kind::endfor_)
            throw std::runtime_error("Unterminated {% for %} block");

          if(result.end_tag->trim_next_text_left)
          {
            trim_next_text_left = true;
          }

          children.push_back(element{std::move(block)});
        }
        else if(trimmed.starts_with("include ") or trimmed == "include")
        {
          auto arg = trim(trimmed.substr(7));
          children.push_back(element{include_block{parse_string_literal(arg, "include")}});

          if(right_trim)
          {
            trim_next_text_left = true;
          }
        }
        else if(trimmed.starts_with("block ") or trimmed == "block")
        {
          auto name = trim(trimmed.substr(5));
          if(name.empty())
          {
            throw std::runtime_error("Missing name in {% block %}");
          }
          if(std::ranges::any_of(name, is_trim_char))
          {
            throw std::runtime_error(std::format("Malformed {{% block %}} name: '{}'", name));
          }

          block_block block;
          block.name = name;

          auto result    = parse_children(input, right_trim);
          block.children = std::move(result.children);

          if(not result.end_tag or result.end_tag->kind != block_end_kind::endblock_)
          {
            throw std::runtime_error("Unterminated {% block %} block");
          }

          if(not result.end_tag->tag_content.empty() and result.end_tag->tag_content != name)
          {
            throw std::runtime_error(
                std::format(
                    "Mismatched {{% endblock %}}: expected '{}', got '{}'", name,
                    result.end_tag->tag_content));
          }

          if(result.end_tag->trim_next_text_left)
          {
            trim_next_text_left = true;
          }

          children.push_back(element{std::move(block)});
        }
        else
        {
          throw std::runtime_error(std::format("Unknown block tag: '{}'", trimmed));
        }
      }
      else
      {
        // '{' alone: treat as literal text
        children.push_back(text{input.substr(0, 1)});
        input.remove_prefix(1);
      }
    }

    return {std::move(children), std::nullopt};
  }

  // name -> block body, pointing into the owning template's children. std::map is not usable
  // here because parse() is constexpr.
  using block_map = std::vector<std::pair<std::string_view, const std::vector<element>*>>;

  constexpr void index_blocks(const std::vector<element>& children, block_map& blocks);

  constexpr const std::vector<element>* find_block(const block_map& blocks, std::string_view name)
  {
    for(const auto& [key, body] : blocks)
    {
      if(key == name)
      {
        return body;
      }
    }
    return nullptr;
  }

  } // namespace detail

  // A parsed template. Every node holds string_view slices into the source it was parsed from,
  // so a template_ never outlives that source - see `environment`, which owns both.
  // Copying is explicit through clone(): the copy still points at the same source, and `blocks`
  // has to be re-indexed against the new children.
  struct template_
  {
    using block_map = detail::block_map;

    std::vector<detail::element>    children;
    std::optional<std::string_view> extends;
    block_map                       blocks; // indexes `children`

    constexpr template_()                            = default;
    constexpr template_(template_&&)                 = default;
    constexpr template_& operator=(template_&&)      = default;
    constexpr template_(const template_&)            = delete;
    constexpr template_& operator=(const template_&) = delete;

    // Copies the tree, re-indexing the blocks. The source string is shared, not copied.
    constexpr template_ clone() const
    {
      template_ result;
      result.children = children;
      result.extends  = extends;
      detail::index_blocks(result.children, result.blocks);
      return result;
    }
  };

  namespace detail
  {

  // Indexes every {% block %} of the tree, at any depth, by name.
  constexpr void index_blocks(const std::vector<element>& children, block_map& blocks)
  {
    for(const auto& child : children)
    {
      std::visit(
          [&]<typename T>(const T& v) {
            if constexpr(decays_to_c<T, block_block>)
            {
              if(find_block(blocks, v.name) != nullptr)
              {
                throw std::runtime_error(std::format("Duplicate block name: '{}'", v.name));
              }
              blocks.emplace_back(v.name, &v.children);
              index_blocks(v.children, blocks);
            }
            else if constexpr(decays_to_c<T, if_block>)
            {
              index_blocks(v.then_children, blocks);
              index_blocks(v.else_children, blocks);
            }
            else if constexpr(decays_to_c<T, for_block>)
            {
              index_blocks(v.children, blocks);
            }
          },
          child);
    }
  }

  } // namespace detail

  constexpr template_ parse(std::string_view input)
  {
    template_ tmpl;
    bool      trim_left_after_extends = false;

    // {% extends "base" %} is a property of the template, and must be its first meaningful tag.
    {
      auto probe = input;
      while(true)
      {
        detail::trim_left(probe);
        if(!probe.starts_with("{#"))
        {
          break;
        }
        // leading comments are not meaningful tags, skip them
        auto scan = probe;
        scan.remove_prefix(2);
        detail::consume_until(scan, "#}");
        probe = scan;
      }

      if(probe.starts_with("{%"))
      {
        auto scan = probe;
        scan.remove_prefix(2);
        if(!scan.empty() and scan.front() == '-')
        {
          scan.remove_prefix(1);
        }

        auto tag_content = detail::consume_until(scan, "%}");
        bool right_trim  = false;
        if(!tag_content.empty() and tag_content.back() == '-')
        {
          right_trim = true;
          tag_content.remove_suffix(1);
        }

        auto trimmed = trim(tag_content);
        if(trimmed.starts_with("extends ") or trimmed == "extends")
        {
          auto arg     = trim(trimmed.substr(7));
          tmpl.extends = detail::parse_string_literal(arg, "extends");

          input                   = scan;
          trim_left_after_extends = right_trim;
        }
      }
    }

    auto result = detail::parse_children(input, trim_left_after_extends);
    if(result.end_tag)
      throw std::runtime_error("Unexpected block-end tag at top level");

    tmpl.children = std::move(result.children);
    detail::index_blocks(tmpl.children, tmpl.blocks);
    return tmpl;
  }

  namespace detail
  {

  // Render-time state that is not carried by the context: the environment used to resolve
  // {% include %} / {% extends %}, plus the inheritance and cycle bookkeeping.
  struct render_state
  {
    environment*                  env = nullptr;
    std::vector<std::string_view> include_stack;
    block_map                     block_overrides;
  };

  // Builds the {{ loop }} of a {% for %} body, chaining to the enclosing loop if there is one.
  // Call before pushing the body's scope, so "loop" still resolves to the parent.
  template <typename ContextT> expr::loop_info make_loop_info(std::size_t length, ContextT& ctx)
  {
    return expr::loop_info{
        .index0 = 0,
        .index  = 1,
        .first  = true,
        .length = int(length),
        .last   = length == 1,
        .parent = ctx.template get<expr::loop_info>("loop"),
    };
  }

  constexpr void advance_loop(expr::loop_info& loop)
  {
    ++loop.index0;
    ++loop.index;
    loop.first = false;
    loop.last  = (loop.index0 == loop.length - 1);
  }

  template <typename OutputIt, typename ContextT>
  OutputIt render_children_to(
      OutputIt                    out,
      const std::vector<element>& children,
      ContextT&                   ctx,
      render_state*               state = nullptr);

  // Defined in <reflex/jinja/environment.hpp>, once `environment` is complete.
  template <typename OutputIt, typename ContextT>
  OutputIt
      render_include_to(OutputIt out, std::string_view name, ContextT& ctx, render_state& state);

  template <typename OutputIt, typename ContextT>
  OutputIt render_element_to(OutputIt out, const element& elem, ContextT& ctx, render_state* state)
  {
    return std::visit(
        [&]<typename T>(const T& v) -> OutputIt {
          if constexpr(decays_to_c<T, text>)
          {
            return std::format_to(out, "{}", v.content);
          }
          else if constexpr(decays_to_c<T, expression>)
          {
            auto value = expr::evaluate(v.expr, ctx);
            using U    = std::decay_t<decltype(value)>;
            if constexpr(std::formattable<U, char>)
            {
              std::format_to(out, "{}", value);
            }
            else
            {
              throw runtime_error(
                  "Value of type {} is not formattable", display_string_of(dealias(^^U)));
            }
            return out;
          }
          else if constexpr(decays_to_c<T, if_block>)
          {
            const bool cond = expr::evaluate_bool(v.condition, ctx);
            return render_children_to(out, cond ? v.then_children : v.else_children, ctx, state);
          }
          else if constexpr(decays_to_c<T, include_block>)
          {
            if(state == nullptr or state->env == nullptr)
            {
              throw std::runtime_error("{% include %} requires an environment");
            }
            return render_include_to(out, v.name, ctx, *state);
          }
          else if constexpr(decays_to_c<T, block_block>)
          {
            const auto* body = &v.children;
            if(state != nullptr)
            {
              if(const auto* override_body = find_block(state->block_overrides, v.name);
                 override_body != nullptr)
              {
                body = override_body;
              }
            }
            return render_children_to(out, *body, ctx, state);
          }
          else if constexpr(decays_to_c<T, for_block>)
          {
            // Evaluate the iterable as a full expression so filters/pipes work.
            auto iterable_val = expr::evaluate(v.iterable, ctx);

            std::visit(
                [&]<typename U>(U&& it) {
                  using V = std::decay_t<U>;
                  if constexpr(seq_c<V>)
                  {
                    // === sequence: {% for item in list %}
                    auto loop  = make_loop_info(it.size(), ctx);
                    auto scope = ctx.push_locals();
                    scope.set("loop", std::ref(loop));
                    for(auto& item : it)
                    {
                      scope.set(v.loop_vars[0], std::ref(item));
                      out = render_children_to(out, v.children, ctx, state);
                      advance_loop(loop);
                    }
                  }
                  else if constexpr(map_c<V>)
                  {
                    // === mapping: {% for k, v in obj %}
                    auto loop  = make_loop_info(it.size(), ctx);
                    auto scope = ctx.push_locals();
                    scope.set("loop", std::ref(loop));
                    for(const auto& [key, val] : it)
                    {
                      if(v.loop_vars.size() == 1)
                      {
                        // {% for k in map %} — bind key and value
                        scope.set(v.loop_vars[0], key);
                      }
                      else
                      {
                        // {% for k, v in map %} — bind key and value
                        scope.set(v.loop_vars[0], key);
                        scope.set(v.loop_vars[1], val);
                      }
                      out = render_children_to(out, v.children, ctx, state);
                      advance_loop(loop);
                    }
                  }
                  else
                  {
                    throw std::runtime_error(
                        std::format(
                            "Value of '{}' is not iterable ({})", v.iterable,
                            display_string_of(^^V)));
                  }
                },
                iterable_val);

            return out;
          }
          else
          {
            static_assert(false, "Non-exhaustive visitor!");
          }
        },
        elem);
  }

  template <typename OutputIt, typename ContextT>
  OutputIt render_children_to(
      OutputIt                    out,
      const std::vector<element>& children,
      ContextT&                   ctx,
      render_state*               state)
  {
    for(const auto& child : children)
    {
      out = render_element_to(out, child, ctx, state);
    }
    return out;
  }

  } // namespace detail

  template <typename... Ts> using context = expr::context<Ts...>;
  using basic_context                     = context<>;

  // Renders without an environment: {% include %} and {% extends %} cannot be resolved, use
  // environment::render_to for templates that need them.
  template <typename OutputIt, typename ContextT = basic_context>
  OutputIt render_to(OutputIt out, const template_& tmpl, ContextT& ctx)
  {
    if(tmpl.extends)
    {
      throw std::runtime_error("{% extends %} requires an environment");
    }
    return detail::render_children_to(out, tmpl.children, ctx, nullptr);
  }

  template <typename ContextT = basic_context>
  inline std::string render(const template_& tmpl, ContextT& ctx)
  {
    std::string result;
    render_to(std::back_inserter(result), tmpl, ctx);
    return result;
  }

} // namespace reflex::jinja

// Needs template_/parse/render_children_to, hence the include at the very bottom.
#include <reflex/jinja/environment.hpp>
