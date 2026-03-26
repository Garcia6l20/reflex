#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <reflex/heapless/string.hpp>

#include <reflex/named_arg.hpp>
#include <reflex/parse.hpp>
#endif

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

    static constexpr auto _max_identifier_size = 32uz;
    using id_string                            = heapless::string<_max_identifier_size>;

    using context_type = ContextT;
    using value_type   = typename ContextT::value_type;
    using array_type   = typename ContextT::array_type;
    using object_type  = typename ContextT::object_type;

    static consteval std::meta::info __integral_type()
    {
      if(has_template_arguments(dealias(^^value_type)))
      {
        for(auto t_arg : template_arguments_of(dealias(^^value_type)))
        {
          if(is_int_number_type(t_arg))
          {
            return t_arg;
          }
        }
      }
      // lookup in bases
      for(auto base : bases_of(dealias(^^value_type), std::meta::access_context::current()))
      {
        if(has_template_arguments(dealias(^^base)))
        {
          for(auto t_arg : template_arguments_of(type_of(base)))
          {
            if(is_int_number_type(t_arg))
            {
              return t_arg;
            }
          }
        }
      }
      return meta::null;
    }
    static constexpr auto integral_type_info = __integral_type();
    using integral_type                      = typename[:integral_type_info:];

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

        id_string               name;
        std::vector<value_type> args;
        args.push_back(left);

        if(at(token_kind::call))
        {
          name = current.lexeme;
          advance();
          consume(token_kind::lparen);

          while(!at(token_kind::rparen) and !at(token_kind::eof))
          {
            args.push_back(parse_expr());
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
        left = (*ctx)(name, args);
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
        left       = {std::get<bool>(coerce_bool(left)) or std::get<bool>(coerce_bool(right))};
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
        left       = {std::get<bool>(coerce_bool(left)) and std::get<bool>(coerce_bool(right))};
      }
      return left;
    }

    value_type parse_not()
    {
      if(at(token_kind::not_))
      {
        advance();
        return {not std::get<bool>(coerce_bool(parse_not()))};
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
          return {equal(left, parse_add())};
        }
        case token_kind::neq:
        {
          advance();
          return {!equal(left, parse_add())};
        }
        case token_kind::lt:
        {
          advance();
          return {compare(left, parse_add()) < 0};
        }
        case token_kind::le:
        {
          advance();
          return {compare(left, parse_add()) <= 0};
        }
        case token_kind::gt:
        {
          advance();
          return {compare(left, parse_add()) > 0};
        }
        case token_kind::ge:
        {
          advance();
          return {compare(left, parse_add()) >= 0};
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
        left       = is_add ? arith_add(left, right) : arith_sub(left, right);
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
            left = arith_mul(left, right);
            break;
          case token_kind::slash:
            left = arith_div(left, right);
            break;
          case token_kind::percent:
            left = arith_mod(left, right);
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

      serde::object_visit(key, base, [&]<typename M>(M&& member) {
        found = true;
        if constexpr(requires { result = value_type{std::forward<M>(member)}; })
        {
          result = value_type{std::forward<M>(member)};
        }
        else if constexpr(requires { result = std::forward<M>(member); })
        {
          result = std::forward<M>(member);
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

      return std::visit(
          [idx]<typename T>(T const& value) -> value_type {
            using U = std::decay_t<T>;
            if constexpr(seq_c<U>)
            {
              if(idx < value.size())
              {
                if constexpr(requires { value_type{value[idx]}; })
                {
                  return value_type{value[idx]};
                }
                else
                {
                  return value[idx];
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

    value_type parse_primary()
    {
      switch(current.kind)
      {
        case token_kind::integer:
        {
          auto v = parse<std::int64_t>(current.lexeme);
          advance();
          return v;
        }
        case token_kind::real:
        {
          auto v = parse<double>(current.lexeme);
          advance();
          return v;
        }
        case token_kind::boolean:
        {
          auto v = parse<bool>(current.lexeme);
          advance();
          return {v};
        }
        case token_kind::null_:
        {
          advance();
          return {};
        }
        case token_kind::string:
        {
          auto v = parse_string(current.lexeme);
          advance();
          return v;
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

          std::vector<value_type> args;
          while(!at(token_kind::rparen) and !at(token_kind::eof))
          {
            args.push_back(parse_expr());
            if(at(token_kind::comma))
              advance();
          }
          consume(token_kind::rparen);

          if(ctx)
          {
            return parse_postfix((*ctx)(name, args));
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

    // value operations

    static value_type coerce_bool(const value_type& v)
    {
      return std::visit(
          [&]<typename T>(T const& value) -> value_type {
            if constexpr(requires { static_cast<bool>(value); })
            {
              return static_cast<bool>(value);
            }
            else if constexpr(requires {
                                { value.empty() } -> std::same_as<bool>;
                              })
            {
              return !value.empty();
            }
            else
            {
              return false;
            }
          },
          v);
    }

    static bool equal(const value_type& a, const value_type& b)
    {
      return std::visit(
          [&]<typename LHS, typename RHS>(LHS const& lhs, RHS const& rhs) -> bool {
            if constexpr(requires { lhs == rhs; })
            {
              return lhs == rhs;
            }
            else
            {
              return false;
            }
          },
          a, b);
    }

    // Returns negative / zero / positive like strcmp
    static int compare(const value_type& a, const value_type& b)
    {
      static const auto to_double = [](const value_type& v) -> double {
        if constexpr(integral_type_info != meta::null)
        {
          if(auto* i = std::get_if<integral_type>(&v))
          {
            return static_cast<double>(*i);
          }
        }
        if(auto* d = std::get_if<double>(&v))
        {
          return *d;
        }
        throw std::runtime_error("Cannot compare non-numeric values with ordering operators");
      };
      const double la = to_double(a);
      const double rb = to_double(b);
      if(la < rb)
      {
        return -1;
      }
      if(la > rb)
      {
        return 1;
      }
      return 0;
    }

    static value_type arith_add(const value_type& a, const value_type& b)
    {
      if constexpr(integral_type_info != meta::null)
      {
        if(auto* la = std::get_if<integral_type>(&a))
        {
          if(auto* rb = std::get_if<integral_type>(&b))
          {
            return {*la + *rb};
          }
        }
      }
      if(auto* la = std::get_if<std::string>(&a))
      {
        if(auto* rb = std::get_if<std::string>(&b))
        {
          return {*la + *rb};
        }
      }
      return {to_double(a) + to_double(b)};
    }

    static value_type arith_sub(const value_type& a, const value_type& b)
    {
      if constexpr(integral_type_info != meta::null)
      {
        if(auto* la = std::get_if<integral_type>(&a))
        {
          if(auto* rb = std::get_if<integral_type>(&b))
          {
            return {*la - *rb};
          }
        }
      }
      return {to_double(a) - to_double(b)};
    }

    static value_type arith_mul(const value_type& a, const value_type& b)
    {
      if constexpr(integral_type_info != meta::null)
      {
        if(auto* la = std::get_if<integral_type>(&a))
        {
          if(auto* rb = std::get_if<integral_type>(&b))
          {
            return {*la * *rb};
          }
        }
      }
      return {to_double(a) * to_double(b)};
    }

    static value_type arith_div(const value_type& a, const value_type& b)
    {
      if constexpr(integral_type_info != meta::null)
      {
        if(auto* la = std::get_if<integral_type>(&a))
        {
          if(auto* rb = std::get_if<integral_type>(&b))
          {
            if(*rb == 0)
            {
              throw std::runtime_error("Integer division by zero");
            }
            return {*la / *rb};
          }
        }
      }
      double rb = to_double(b);
      if(rb == 0.0)
      {
        throw std::runtime_error("Division by zero");
      }
      return {to_double(a) / rb};
    }

    static value_type arith_mod(const value_type& a, const value_type& b)
    {
      if constexpr(integral_type_info != meta::null)
      {
        if(auto* la = std::get_if<integral_type>(&a))
        {
          if(auto* rb = std::get_if<integral_type>(&b))
          {
            if(*rb == 0)
            {
              throw std::runtime_error("Modulo by zero");
            }
            return {*la % *rb};
          }
        }
      }
      throw std::runtime_error("'%' requires integer operands");
    }

    static double to_double(const value_type& v)
    {
      if constexpr(integral_type_info != meta::null)
      {
        if(auto* i = std::get_if<integral_type>(&v))
        {
          return static_cast<double>(*i);
        }
      }
      if(auto* d = std::get_if<double>(&v))
      {
        return *d;
      }
      throw std::runtime_error("Expected numeric value");
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
    return std::get<bool>(parser<ContextT>::coerce_bool(v));
  }

} // namespace reflex::jinja::expr

namespace reflex::serde
{
// Generic specialization to allow visiting `std::optional<T>` where `T` is
// itself visitable as an object. When the optional is empty the visitor is
// invoked with `poly::null`.
template <typename T>
  requires(object_visitable_c<std::remove_reference_t<T>>)
struct object_visitor<std::optional<T>>
{
  template <typename Fn, typename Agg>
  static inline constexpr decltype(auto) operator()(Fn&& fn, std::string_view key, Agg&& agg)
  {
    using Inner = std::remove_reference_t<T>;
    if(agg.has_value())
    {
      return object_visitor<Inner>{}(std::forward<Fn>(fn), key, std::forward<Agg>(agg).value());
    }
    else
    {
      return std::forward<Fn>(fn)(poly::null);
    }
  }
};
} // namespace reflex::serde