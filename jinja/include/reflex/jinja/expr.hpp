#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <reflex/heapless/string.hpp>
#include <reflex/scope_guard.hpp>

#include <reflex/named_arg.hpp>
#include <reflex/parse.hpp>
#endif

#include <reflex/jinja/builtins.hpp>
#include <reflex/jinja/context.hpp>

#ifdef REFLEX_EXPR_ABORT_ON_NON_MATCHED_ELEMENT
#define REFLEX_EXPR_UNREACHABLE_OR_NULL() \
  [] {                                    \
    std::abort();                         \
    return reflex::poly::null;            \
  }()
#else
#define REFLEX_EXPR_UNREACHABLE_OR_NULL() reflex::poly::null
#endif

REFLEX_EXPORT namespace reflex::jinja::expr
{
  // === Token

  enum class token_kind
  {
    // literals
    integer, // 42, -1
    real,    // 3.14
    string,  // "hello"
    boolean, // true / false
    null_,   // null / none / nil

    // identifiers / calls
    identifier, // foo
    call,       // foo(  — identifier followed immediately by '('

    // comparison operators
    eq,  // ==
    neq, // !=
    lt,  // <
    le,  // <=
    gt,  // >
    ge,  // >=

    // logical operators
    and_, // and / &&
    or_,  // or  / ||
    not_, // not / !

    // arithmetic operators
    plus,    // +
    minus,   // -
    star,    // *
    slash,   // /
    percent, // %
    pipe,    // |

    // punctuation
    lparen, // (
    rparen, // )
    comma,  // ,
    dot,    // .

    // subscript
    lbracket, // [
    rbracket, // ]

    eof,
  };

  struct token
  {
    token_kind       kind;
    std::string_view lexeme; // slice into the original source
  };

  using null_t = serde::json::null_t;

  // default value
  using basic_value = std::variant<null_t, bool, std::int64_t, double, std::string>;

  struct lexer
  {
    std::string_view src;
    std::size_t      pos{0};

    constexpr char peek(std::size_t offset = 0) const noexcept
    {
      return (pos + offset < src.size()) ? src[pos + offset] : '\0';
    }

    constexpr void skip_ws() noexcept
    {
      while(pos < src.size() and is_space(src[pos]))
      {
        ++pos;
      }
    }

    constexpr std::string_view slice(std::size_t start) const noexcept
    {
      return src.substr(start, pos - start);
    }

    constexpr token next()
    {
      skip_ws();
      if(pos >= src.size())
      {
        return {token_kind::eof, {}};
      }

      const std::size_t start = pos;
      const char        c     = src[pos];

      if(c == '"' or c == '\'')
      {
        char delim = c;
        ++pos;
        while(pos < src.size() and src[pos] != delim)
        {
          if(src[pos] == '\\')
          {
            ++pos; // skip escape
          }
          ++pos;
        }
        if(pos < src.size())
        {
          ++pos; // consume closing quote
        }
        return {token_kind::string, slice(start)};
      }

      if(is_digit(c) or (c == '-' and is_digit(peek(1))))
      {
        if(c == '-')
        {
          ++pos;
        }
        while(pos < src.size() and is_digit(src[pos]))
        {
          ++pos;
        }
        bool is_real = false;
        if(pos < src.size() and src[pos] == '.')
        {
          is_real = true;
          ++pos;
          while(pos < src.size() and is_digit(src[pos]))
          {
            ++pos;
          }
        }
        return {is_real ? token_kind::real : token_kind::integer, slice(start)};
      }

      if(is_alpha(c) or c == '_')
      {
        while(pos < src.size() and (is_alphanum(src[pos]) or src[pos] == '_'))
        {
          ++pos;
        }

        auto lex = slice(start);

        // peek ahead for '(' to detect a call
        skip_ws();

        if(lex == "true")
        {
          return {token_kind::boolean, lex};
        }
        if(lex == "false")
        {
          return {token_kind::boolean, lex};
        }
        if(lex == "null" or lex == "none" or lex == "nil")
        {
          return {token_kind::null_, lex};
        }
        if(lex == "and")
        {
          return {token_kind::and_, lex};
        }
        if(lex == "or")
        {
          return {token_kind::or_, lex};
        }
        if(lex == "not")
        {
          return {token_kind::not_, lex};
        }
        if(pos < src.size() and src[pos] == '(')
        {
          return {token_kind::call, lex};
        }

        return {token_kind::identifier, lex};
      }

      // === Two-char operators
      if(pos + 1 < src.size())
      {
        auto two = src.substr(pos, 2);
        if(two == "==")
        {
          pos += 2;
          return {token_kind::eq, two};
        }
        if(two == "!=")
        {
          pos += 2;
          return {token_kind::neq, two};
        }
        if(two == "<=")
        {
          pos += 2;
          return {token_kind::le, two};
        }
        if(two == ">=")
        {
          pos += 2;
          return {token_kind::ge, two};
        }
        if(two == "&&")
        {
          pos += 2;
          return {token_kind::and_, two};
        }
        if(two == "||")
        {
          pos += 2;
          return {token_kind::or_, two};
        }
      }

      // === Single-char operators / punctuation
      ++pos;
      auto lex = slice(start);
      switch(c)
      {
        case '<':
          return {token_kind::lt, lex};
        case '>':
          return {token_kind::gt, lex};
        case '!':
          return {token_kind::not_, lex};
        case '+':
          return {token_kind::plus, lex};
        case '-':
          return {token_kind::minus, lex};
        case '*':
          return {token_kind::star, lex};
        case '/':
          return {token_kind::slash, lex};
        case '%':
          return {token_kind::percent, lex};
        case '|':
          return {token_kind::pipe, lex};
        case '(':
          return {token_kind::lparen, lex};
        case ')':
          return {token_kind::rparen, lex};
        case ',':
          return {token_kind::comma, lex};
        case '.':
          return {token_kind::dot, lex};
        case '[':
          return {token_kind::lbracket, lex};
        case ']':
          return {token_kind::rbracket, lex};
        default:
          throw std::runtime_error(std::format("Unexpected character '{}' in expression", c));
      }
    }
  };

  // === Parser / evaluator
  //
  // Grammar (precedence, low -> high):
  //
  //   expr      := or_expr
  //   expr      := pipe_expr
  //   pipe_expr := or_expr ( '|' ( identifier | call ) )*
  //   or_expr   := and_expr  ( ('or' | '||')  and_expr  )*
  //   and_expr  := not_expr  ( ('and' | '&&') not_expr  )*
  //   not_expr  := ('not'|'!') not_expr  |  cmp_expr
  //   cmp_expr  := add_expr  ( ('=='|'!='|'<'|'<='|'>'|'>=') add_expr )?
  //   add_expr  := mul_expr  ( ('+'|'-') mul_expr )*
  //   mul_expr  := unary     ( ('*'|'/'|'%') unary )*
  //   unary     := '-' unary | primary
  //   primary   := literal | identifier | call | '(' expr ')'
  //
  template <typename ContextT> struct parser
  {
    lexer lex;
    token current;

    using context_type = ContextT;
    using value_type   = typename ContextT::value_type;
    using array_type   = typename ContextT::array_type;
    using object_type  = typename ContextT::object_type;

    static constexpr auto _max_identifier_size = 32uz;
    static constexpr auto _max_call_args       = 8uz;

    using id_string   = heapless::string<_max_identifier_size>;
    using args_vector = heapless::vector<value_type, _max_call_args>;

    // The coercions live in value_ops so a builtin can reach them without instantiating a parser.
    using ops                                = value_ops<value_type>;
    static constexpr auto integral_type_info = ops::integral_type_info;
    using integral_type                      = typename ops::integral_type;

    const context_type* ctx{nullptr};

    explicit parser(std::string_view src, const context_type* c = nullptr) : lex{src}, ctx{c}
    {
      advance();
    }

    // === helpers

    void advance()
    {
      current = lex.next();
    }

    bool at(token_kind k) const noexcept
    {
      return current.kind == k;
    }

    void push_call_arg(args_vector& args, value_type arg)
    {
      if(args.size() >= _max_call_args)
      {
        throw std::runtime_error(
            std::format("Too many function arguments (max {})", _max_call_args));
      }
      args.push_back(std::move(arg));
    }

    token consume(token_kind k)
    {
      if(!at(k))
      {
        throw std::runtime_error(
            std::format("Expected token kind {}, got '{}'", static_cast<int>(k), current.lexeme));
      }
      auto t = current;
      advance();
      return t;
    }

    // === value helpers
    static value_type parse_string(std::string_view s)
    {
      // strip surrounding quotes and handle basic escapes
      std::string result;
      result.reserve(s.size());
      std::size_t i = 1; // skip opening quote
      while(i < s.size() - 1)
      {
        if(s[i] == '\\' and i + 1 < s.size() - 1)
        {
          ++i;
          switch(s[i])
          {
            case 'n':
              result += '\n';
              break;
            case 't':
              result += '\t';
              break;
            case 'r':
              result += '\r';
              break;
            default:
              result += s[i];
              break;
          }
        }
        else
        {
          result += s[i];
        }
        ++i;
      }
      return result;
    }

    // === grammar rules

    value_type parse_expr()
    {
      return parse_pipe();
    }

    value_type parse_pipe()
    {
      auto left = parse_or();

      while(at(token_kind::pipe))
      {
        advance();

        std::string_view name;
        args_vector      args;
        push_call_arg(args, left);

        if(at(token_kind::call))
        {
          name = current.lexeme;
          advance();
          consume(token_kind::lparen);

          while(!at(token_kind::rparen) and !at(token_kind::eof))
          {
            push_call_arg(args, parse_expr());
            if(at(token_kind::comma))
            {
              advance();
            }
          }
          consume(token_kind::rparen);
        }
        else if(at(token_kind::identifier))
        {
          name = current.lexeme;
          advance();
        }
        else
        {
          throw std::runtime_error(
              std::format("Expected function name after pipe, got '{}'", current.lexeme));
        }

        if(!ctx)
        {
          throw std::runtime_error(std::format("Unknown function '{}'", name));
        }
        left = (*ctx)(name, std::span<value_type>{args.data(), args.size()});
      }

      return left;
    }

    value_type parse_or()
    {
      auto left = parse_and();
      while(at(token_kind::or_))
      {
        advance();
        auto right = parse_and();
        left =
            {std::get<bool>(ops::coerce_bool(left)) or std::get<bool>(ops::coerce_bool(right))};
      }
      return left;
    }

    value_type parse_and()
    {
      auto left = parse_not();
      while(at(token_kind::and_))
      {
        advance();
        auto right = parse_not();
        left =
            {std::get<bool>(ops::coerce_bool(left)) and std::get<bool>(ops::coerce_bool(right))};
      }
      return left;
    }

    value_type parse_not()
    {
      if(at(token_kind::not_))
      {
        advance();
        return {not std::get<bool>(ops::coerce_bool(parse_not()))};
      }
      return parse_cmp();
    }

    value_type parse_cmp()
    {
      auto left = parse_add();
      switch(current.kind)
      {
        case token_kind::eq:
        {
          advance();
          return {ops::equal(left, parse_add())};
        }
        case token_kind::neq:
        {
          advance();
          return {!ops::equal(left, parse_add())};
        }
        case token_kind::lt:
        {
          advance();
          return {ops::compare(left, parse_add()) < 0};
        }
        case token_kind::le:
        {
          advance();
          return {ops::compare(left, parse_add()) <= 0};
        }
        case token_kind::gt:
        {
          advance();
          return {ops::compare(left, parse_add()) > 0};
        }
        case token_kind::ge:
        {
          advance();
          return {ops::compare(left, parse_add()) >= 0};
        }
        default:
          break;
      }
      return left;
    }

    value_type parse_add()
    {
      auto left = parse_mul();
      while(at(token_kind::plus) or at(token_kind::minus))
      {
        bool is_add = at(token_kind::plus);
        advance();
        auto right = parse_mul();
        left       = is_add ? ops::arith_add(left, right) : ops::arith_sub(left, right);
      }
      return left;
    }

    value_type parse_mul()
    {
      auto left = parse_unary();
      while(at(token_kind::star) or at(token_kind::slash) or at(token_kind::percent))
      {
        auto op = current.kind;
        advance();
        auto right = parse_unary();
        switch(op)
        {
          case token_kind::star:
            left = ops::arith_mul(left, right);
            break;
          case token_kind::slash:
            left = ops::arith_div(left, right);
            break;
          case token_kind::percent:
            left = ops::arith_mod(left, right);
            break;
          default:
            break;
        }
      }
      return left;
    }

    value_type parse_unary()
    {
      if(at(token_kind::minus))
      {
        advance();
        auto v = parse_unary();
        if constexpr(integral_type_info != meta::null)
        {
          if(auto* i = std::get_if<integral_type>(&v))
          {
            return {-*i};
          }
        }
        if(auto* d = std::get_if<double>(&v))
        {
          return {-*d};
        }
        throw std::runtime_error("Unary '-' applied to non-numeric value");
      }
      return parse_primary();
    }

    static std::int64_t to_index(const value_type& v)
    {
      if constexpr(integral_type_info != meta::null)
      {
        if(auto* i = std::get_if<integral_type>(&v))
        {
          return static_cast<std::int64_t>(*i);
        }
      }
      throw std::runtime_error("Subscript index must be an integer");
    }

    static value_type access_member(const value_type& base, std::string_view key)
    {
      bool       found  = false;
      value_type result = {};

      serde::object_visit_flat(key, base, [&]<typename M>(M&& member) {
        found = true;
        if constexpr(requires { result = value_type{std::forward<M>(member)}; })
        {
          result = value_type{std::forward<M>(member)};
        }
        else if constexpr(requires { result = std::forward<M>(member); })
        {
          result = std::forward<M>(member);
        }
        else if constexpr(std::is_lvalue_reference_v<M> and requires { result = std::ref(member); })
        {
          result = std::ref(member);
        }
        else if constexpr(seq_c<std::decay_t<M>>)
        {
          result = member
                 | std::views::transform([]<typename E>(E&& elem) -> value_type {
                     if constexpr(requires { value_type{std::ref(elem)}; })
                     {
                       return value_type{std::ref(elem)};
                     }
                     else if constexpr(requires { value_type{std::forward<E>(elem)}; })
                     {
                       return value_type{std::forward<E>(elem)};
                     }
                     else
                     {
                       return REFLEX_EXPR_UNREACHABLE_OR_NULL();
                     }
                   })
                 | std::ranges::to<array_type>();
        }
        else
        {
          result = REFLEX_EXPR_UNREACHABLE_OR_NULL();
        }
      });

      if(!found)
      {
        return {};
      }
      return result;
    }

    static value_type access_index(const value_type& base, std::int64_t index)
    {
      if(index < 0)
      {
        return {};
      }
      const auto idx = static_cast<std::size_t>(index);

      return reflex::visit(
          [idx]<typename T>(T&& value) -> value_type {
            using U = std::decay_t<T>;
            if constexpr(seq_c<U>)
            {
              if(idx < value.size())
              {
                if constexpr(requires { value_type{std::ref(value[idx])}; })
                {
                  return value_type{std::ref(value[idx])};
                }
                else if constexpr(requires { value_type{value[idx]}; })
                {
                  return value_type{value[idx]};
                }
                else
                {
                  return {};
                }
              }
              return {};
            }
            else
            {
              return {};
            }
          },
          base);
    }

    value_type parse_postfix(value_type base)
    {
      while(true)
      {
        if(at(token_kind::dot))
        {
          advance();
          auto member = consume(token_kind::identifier).lexeme;
          base        = access_member(base, member);
          continue;
        }

        if(at(token_kind::lbracket))
        {
          advance();
          auto index_expr = parse_expr();
          consume(token_kind::rbracket);
          base = access_index(base, to_index(index_expr));
          continue;
        }

        break;
      }
      return base;
    }

    template <typename T> static T parse_literal(std::string_view lexeme)
    {
      return reflex::parse_or_throw<T>(lexeme);
    }

    value_type parse_primary()
    {
      switch(current.kind)
      {
        case token_kind::integer:
        {
          reflex::scope_guard _ = [this] { advance(); };
          return parse_literal<std::int64_t>(current.lexeme);
        }
        case token_kind::real:
        {
          reflex::scope_guard _ = [this] { advance(); };
          return parse_literal<double>(current.lexeme);
        }
        case token_kind::boolean:
        {
          reflex::scope_guard _ = [this] { advance(); };
          return parse_literal<bool>(current.lexeme);
        }
        case token_kind::null_:
        {
          advance();
          return {};
        }
        case token_kind::string:
        {
          reflex::scope_guard _ = [this] { advance(); };
          return parse_string(current.lexeme);
        }
        case token_kind::identifier:
        {
          id_string name{current.lexeme};
          advance();

          // Resolve the leading dotted chain through context so aggregate members remain readable.
          while(at(token_kind::dot))
          {
            advance();
            auto member = consume(token_kind::identifier).lexeme;
            name += ".";
            name += member;
          }

          auto value = ctx ? (*ctx)[name] : value_type{};
          return parse_postfix(std::move(value));
        }
        case token_kind::call:
        {
          auto name = current.lexeme;
          advance();
          consume(token_kind::lparen);

          args_vector args;
          while(!at(token_kind::rparen) and !at(token_kind::eof))
          {
            push_call_arg(args, parse_expr());
            if(at(token_kind::comma))
              advance();
          }
          consume(token_kind::rparen);

          if(ctx)
          {
            return parse_postfix((*ctx)(name, std::span<value_type>{args.data(), args.size()}));
          }
          throw std::runtime_error(std::format("Unknown function '{}'", name));
        }
        case token_kind::lparen:
        {
          advance();
          auto v = parse_expr();
          consume(token_kind::rparen);
          return parse_postfix(std::move(v));
        }
        default:
          throw std::runtime_error(
              std::format("Unexpected token '{}' in expression", current.lexeme));
      }
    }

  };

  template <typename ContextT = context<>>
  inline typename ContextT::value_type evaluate(std::string_view src, const ContextT& ctx = {})
  {
    auto p = parser<ContextT>{trim(src), &ctx};
    auto v = p.parse_expr();
    p.consume(token_kind::eof);
    return v;
  }

  template <typename ContextT = context<>>
  inline bool evaluate_bool(std::string_view src, const ContextT& ctx = {})
  {
    auto v = evaluate<ContextT>(src, ctx);
    return std::get<bool>(value_ops<typename ContextT::value_type>::coerce_bool(v));
  }

} // namespace reflex::jinja::expr

namespace reflex::serde
{
/**
 * @brief Names a member of `std::optional<T>`'s payload, for a `T` itself visitable as an object.
 * @param fn Called with the member @p key names in @p agg's payload, and left uncalled when @p agg
 *           is disengaged: there is no payload then, and so no member of one to name. That is how
 *           every unreachable path is reported. Handing @p fn `poly::null` instead would tell
 *           `object_visit` the hop succeeded, and the walk would ask a member-less value for the
 *           next segment.
 * @param key Name of the member to visit.
 * @param agg The optional to reach through.
 */
template <typename T>
  requires(object_visitable_c<std::remove_reference_t<T>>)
struct object_visitor<std::optional<T>>
{
  template <typename Fn, typename Agg>
  static inline constexpr void operator()(Fn&& fn, std::string_view key, Agg&& agg)
  {
    using Inner = std::remove_reference_t<T>;
    static_assert(
        std::is_void_v<decltype(object_visitor<Inner>{}(
            std::forward<Fn>(fn), key, std::forward<Agg>(agg).value()))>,
        "a disengaged optional has no member to hand over, so there is nothing to return: give "
        "this visit a callback returning void");
    if(agg.has_value())
    {
      object_visitor<Inner>{}(std::forward<Fn>(fn), key, std::forward<Agg>(agg).value());
    }
  }
};
} // namespace reflex::serde