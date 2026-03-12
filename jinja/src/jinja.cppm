export module reflex.jinja;

export import :expr;

export import reflex.core;
export import reflex.serde.json;

import std;

export namespace reflex::jinja
{

namespace detail
{

struct text;
struct expression;
struct if_block;
struct for_block;

using element = std::variant<text, expression, if_block, for_block>;

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

enum class block_end_kind
{
  elif_,
  else_,
  endif_,
  endfor_,
};

struct block_end
{
  block_end_kind   kind;
  std::string_view tag_content; // carries the elif condition string_view
};

struct children_result
{
  std::vector<element>     children;
  std::optional<block_end> end_tag;
};

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

constexpr children_result parse_children(std::string_view& input);

constexpr if_block parse_if_block(std::string_view condition, std::string_view& input)
{
  if_block block;
  block.condition = condition;

  auto result = parse_children(input);

  if(!result.end_tag)
  {
    throw std::runtime_error("Unterminated {% if %} / {% elif %} block");
  }

  block.then_children = std::move(result.children);

  switch(result.end_tag->kind)
  {
    case block_end_kind::endif_:
      // no else branch
      break;

    case block_end_kind::else_:
    {
      auto else_result = parse_children(input);
      if(!else_result.end_tag || else_result.end_tag->kind != block_end_kind::endif_)
      {
        throw std::runtime_error("Expected {% endif %} after {% else %}");
      }
      block.else_children = std::move(else_result.children);
      break;
    }

    case block_end_kind::elif_:
    {
      // Recursively build the elif as a nested if_block in else_children.
      block.else_children = {element{parse_if_block(result.end_tag->tag_content, input)}};
      break;
    }

    default:
      throw std::runtime_error("Unexpected block-end tag inside {% if %}");
  }

  return block;
}

constexpr children_result parse_children(std::string_view& input)
{
  std::vector<element> children;

  while(!input.empty())
  {
    auto next = input.find('{');
    if(next == std::string_view::npos)
    {
      if(!input.empty())
      {
        children.push_back(text{input});
      }
      input = {};
      break;
    }

    // Collect raw text before the tag
    if(next > 0)
    {
      children.push_back(text{input.substr(0, next)});
      input.remove_prefix(next);
    }

    if(input.size() < 2)
    {
      children.push_back(text{input});
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
      auto tag_content = consume_until(input, "%}");
      auto trimmed     = trim(tag_content);

      if(trimmed == "endif")
      {
        return {
            std::move(children), block_end{block_end_kind::endif_, {}}
        };
      }

      if(trimmed == "endfor")
      {
        return {
            std::move(children), block_end{block_end_kind::endfor_, {}}
        };
      }

      if(trimmed == "else")
      {
        return {
            std::move(children), block_end{block_end_kind::else_, {}}
        };
      }

      if(trimmed.starts_with("elif"))
      {
        auto cond = trim(trimmed.substr(4));
        if(cond.empty())
          throw std::runtime_error("Empty condition in {% elif %}");
        return {
            std::move(children), block_end{block_end_kind::elif_, cond}
        };
      }

      if(trimmed.starts_with("if ") || trimmed == "if")
      {
        auto cond = trim(trimmed.substr(2));
        if(cond.empty())
          throw std::runtime_error("Empty condition in {% if %}");
        children.push_back(element{parse_if_block(cond, input)});
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

        auto result    = parse_children(input);
        block.children = std::move(result.children);

        if(!result.end_tag || result.end_tag->kind != block_end_kind::endfor_)
          throw std::runtime_error("Unterminated {% for %} block");

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

} // namespace detail

struct template_
{
  std::vector<detail::element> children;
};

constexpr template_ parse(std::string_view input)
{
  auto result = detail::parse_children(input);
  if(result.end_tag)
    throw std::runtime_error("Unexpected block-end tag at top level");
  return template_{std::move(result.children)};
}

namespace detail
{

template <typename OutputIt, typename ContextT>
OutputIt
    render_children_to(OutputIt out, const std::vector<element>& children, const ContextT& ctx);

template <typename OutputIt, typename ContextT>
OutputIt render_element_to(OutputIt out, const element& elem, const ContextT& ctx)
{
  return std::visit(
      reflex::match{
          [&](const text& t) -> OutputIt { return std::format_to(out, "{}", t.content); },
          [&](const expression& e) -> OutputIt {
            if(auto value = ctx.get(e.expr); value)
            {
              return std::format_to(out, "{}", value.value());
            }
            return out;
          },
          [&](const if_block& ib) -> OutputIt {
            const bool cond = expr::evaluate_bool(ib.condition, ctx);
            return render_children_to(out, cond ? ib.then_children : ib.else_children, ctx);
          },
          [&](const for_block& fb) -> OutputIt {
            decltype(auto) value = ctx.get(fb.iterable);

            if(!value)
              return out;

            return reflex::visit(
                match{
                    // === sequence: {% for item in list %}
                    [&](serde::json::detail::seq_c auto const& arr) -> OutputIt {
                      for(const auto& item : arr)
                      {
                        ContextT local = ctx;
                        local.set(std::string(fb.loop_vars[0]), item);
                        out = render_children_to(out, fb.children, local);
                      }
                      return out;
                    },
                    // === mapping: {% for k, v in obj %}
                    [&](serde::json::detail::map_c auto const& map) -> OutputIt {
                      for(const auto& [key, val] : map)
                      {
                        ContextT local = ctx;
                        if(fb.loop_vars.size() == 1)
                        {
                          // {% for k in map %} — bind key and value
                          local.set(std::string(fb.loop_vars[0]), key);
                        }
                        else
                        {
                          // {% for k, v in map %} — bind key and value
                          local.set(std::string(fb.loop_vars[0]), key);
                          local.set(std::string(fb.loop_vars[1]), val);
                        }
                        out = render_children_to(out, fb.children, local);
                      }
                      return out;
                    },
                    [&](const auto& item) -> OutputIt {
                      throw std::runtime_error(
                          std::format(
                              "Value of '{}' is not iterable ({}): {}", fb.iterable,
                              display_string_of(type_of(^^item)), item));
                    },
                },
                *value);
          },
      },
      elem);
}

template <typename OutputIt, typename ContextT>
OutputIt render_children_to(OutputIt out, const std::vector<element>& children, const ContextT& ctx)
{
  for(const auto& child : children)
  {
    out = render_element_to(out, child, ctx);
  }
  return out;
}

} // namespace detail

using basic_context = expr::context<expr::basic_value>;
using json_context  = expr::context<serde::json::value>;

template <typename OutputIt, typename ContextT = basic_context>
OutputIt render_to(OutputIt out, const template_& tmpl, const ContextT& ctx)
{
  return detail::render_children_to(out, tmpl.children, ctx);
}

template <typename ContextT = basic_context>
inline std::string render(const template_& tmpl, const ContextT& ctx)
{
  std::string result;
  render_to(std::back_inserter(result), tmpl, ctx);
  return result;
}

} // namespace reflex::jinja
