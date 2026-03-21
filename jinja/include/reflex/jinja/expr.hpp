#pragma once

#include <reflex/core.hpp>
#include <reflex/serde.hpp>
#include <reflex/serde/json.hpp>

#include <functional>

namespace reflex::jinja::expr
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

// === Context / callable registry

struct loop_info
{
  int                       index0;
  int                       index;
  bool                      first;
  int                       length;
  bool                      last;
  std::optional<loop_info&> parent;
};

// scans the types of all nonstatic data members of an aggregate, including nested aggregates and
// element types of sequences and maps
consteval auto scan_object_types(
    std::meta::info           agg,
    std::meta::access_context ctx = std::meta::access_context::current())
    -> std::vector<std::meta::info>
{
  std::vector<std::meta::info> types;
  for(auto m : std::meta::nonstatic_data_members_of(agg, ctx))
  {
    auto t = dealias(type_of(m));
    if(std::ranges::contains(types, t))
    {
      continue;
    }
    if(meta::eval_concept(
           ^^seq_c, {
                        t}))
    {
      // for sequences, also include the element type
      auto value_type = dealias(meta::member_named(t, "value_type"));
      if(!std::ranges::contains(types, value_type))
      {
        types.push_back(value_type);
        if(is_aggregate_type(value_type))
        {
          // if the element type is an aggregate, also include its nonstatic data member types
          for(auto nt : scan_object_types(value_type, ctx))
          {
            if(!std::ranges::contains(types, nt))
            {
              types.push_back(nt);
            }
          }
        }
      }
    }
    else if(
        meta::eval_concept(
            ^^map_c, {
                         t}))
    {
      // for maps, also include the key and value types
      auto key_type = dealias(meta::member_named(t, "key_type"));
      if(!std::ranges::contains(types, key_type))
      {
        types.push_back(key_type);
      }
      auto mapped_type = dealias(meta::member_named(t, "mapped_type"));
      if(!std::ranges::contains(types, mapped_type))
      {
        types.push_back(mapped_type);
        if(is_aggregate_type(mapped_type))
        {
          // if the mapped type is an aggregate, also include its nonstatic data member types
          for(auto nt : scan_object_types(mapped_type, ctx))
          {
            if(!std::ranges::contains(types, nt))
            {
              types.push_back(nt);
            }
          }
        }
      }
    }
    else
    {
      // regular type
      types.push_back(t);
      if(is_aggregate_type(t))
      {
        // if the type is an aggregate, also include its nonstatic data member types
        for(auto nt : scan_object_types(t, ctx))
        {
          if(!std::ranges::contains(types, nt))
          {
            types.push_back(nt);
          }
        }
      }
    }
  }
  return types;
}

// A callable registered with the evaluator.
template <typename... Ts> struct context
{
  static constexpr auto _mandatory_types =
      std::array{^^bool, ^^int, ^^double, decay(^^std::string), add_lvalue_reference(^^loop_info)};

  context() = default;

  context(named_arg<Ts>... args)
    requires(sizeof...(Ts) > 0)
  {
    (set(args.name, std::ref(args.value)), ...);
  }

  using value_type    = typename[:[] consteval {
    auto types = std::vector{std::from_range, _mandatory_types};

    template for(constexpr auto t : {^^Ts...})
    {
      if(!std::ranges::contains(types, t))
      {
        types.push_back(t);
        auto dt = decay(t);
        if(is_aggregate_type(dt))
        {
          for(auto nt : scan_object_types(dt))
          {
            if(is_aggregate_type(nt))
            {
              auto ref_type = add_lvalue_reference(nt);
              if(!std::ranges::contains(types, ref_type))
              {
                types.push_back(ref_type);
              }
            }
            else
            {
              if(!std::ranges::contains(types, decay(nt)))
              {
                types.push_back(decay(nt));
              }
            }
          }
        }
      }
    }
    return substitute(^^poly::var, types);
  }():];
  using object_type   = typename value_type::obj_type;
  using array_type    = typename value_type::arr_type;
  using function_type = std::function<value_type(std::span<const value_type>)>;

  object_type              global_vars;
  std::vector<object_type> local_vars;

  std::unordered_map<std::string, function_type> funcs;

  context& set(std::string_view name, value_type v)
  {
    global_vars.insert_or_assign(std::string{name}, std::move(v));
    return *this;
  }

  context& def(std::string name, function_type fn)
  {
    funcs.insert_or_assign(std::move(name), std::move(fn));
    return *this;
  }

  value_type operator()(std::string_view fname, std::span<const value_type> args) const
  {
    if(auto it =
           std::ranges::find_if(funcs, [fname](auto const& pair) { return pair.first == fname; });
       it != funcs.end())
    {
      return it->second(args);
    }
    throw std::runtime_error("Context is not callable");
  }

  decltype(auto) operator[](std::string_view name) const noexcept
  {
    value_type result = poly::null;
    visit(name, [&]<typename V>(V&& v) {
      using U = std::decay_t<V>;
      if constexpr(requires { result = v; })
      {
        result = v;
      }
      else if constexpr(seq_c<U> and requires { value_type{std::ref(v)}; })
      {
        result = v
               | std::views::transform([](auto&& elem) { return std::ref(elem); })
               | std::ranges::to<array_type>();
      }
      else
      {
        throw runtime_error(std::format("Variable '{}' is not readable", name));
      }
    });
    return result;
  }

  void get(std::string_view name, value_type& result) const
  {
    result = operator[](name);
  }

  template <typename T> std::optional<T&> get(std::string_view name) const
  {
    std::optional<T&> result;
    visit(name, [&result]<typename U>(U&& v) {
      if constexpr(requires { result = v; })
      {
        result = v;
      }
      else
      {
        result = std::nullopt;
      }
    });
    return result;
  }

  // value_type& operator[](std::string_view name) noexcept
  // {
  //   return visit(name, [](auto&& v) -> auto& { return v; });
  // }

  void visit(this auto& self, std::string_view dotted_keys, auto&& fn) noexcept
  {
    bool found = false;
    for(auto&& locals : self.local_vars | std::views::reverse)
    {
      serde::object_visit(dotted_keys, locals, [&found, &fn]<typename T>(T&& v) {
        found = true;
        if constexpr(meta::is_template_instance_of(decay(^^T), ^^std::optional))
        {
          if(v.has_value())
          {
            std::forward<decltype(fn)>(fn)(std::forward<T>(v).value());
          }
          else
          {
            std::forward<decltype(fn)>(fn)(poly::null);
          }
        }
        else
        {
          std::forward<decltype(fn)>(fn)(std::forward<T>(v));
        }
      });
      if(found)
      {
        return;
      }
    }
    serde::object_visit(dotted_keys, self.global_vars, std::forward<decltype(fn)>(fn));
  }

  struct [[nodiscard("local_scope_guard must be used to avoid dangling local variables")]]
  local_scope_guard
  {
    context&    ctx;
    std::size_t index;
    local_scope_guard(context& c) : ctx{c}, index{ctx.local_vars.size()}
    {
      ctx.local_vars.emplace_back();
    }

    decltype(auto) set(std::string_view name, value_type v)
    {
      ctx.local_vars.at(index).insert_or_assign(std::string{name}, std::move(v));
      return *this;
    }

    ~local_scope_guard()
    {
      ctx.local_vars.pop_back();
    }
  };

  local_scope_guard push_locals()
  {
    return local_scope_guard{*this};
  }

  void dump() const
  {
    std::println("Globals:");
    for(const auto& [k, v] : global_vars)
    {
      std::println("  {}: {}", k, v);
    }
    std::println("Locals:");
    for(const auto& [index, locals] : local_vars | std::views::enumerate)
    {
      std::println("  Scope {}@{}:", index, static_cast<const void*>(&locals));
      for(const auto& [k, v] : locals)
      {
        std::println("    {}: {}", k, v);
      }
    }
  }
};

// === Parser / evaluator
//
// Grammar (precedence, low -> high):
//
//   expr      := or_expr
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
    return parse_or();
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

    try
    {
      serde::object_visit(key, base, [&](auto&& member) {
        found = true;
        if constexpr(requires { result = value_type{std::forward<decltype(member)>(member)}; })
        {
          result = value_type{std::forward<decltype(member)>(member)};
        }
        else if constexpr(requires { result = std::forward<decltype(member)>(member); })
        {
          result = std::forward<decltype(member)>(member);
        }
        else
        {
          throw std::runtime_error(std::format("Member '{}' is not readable", key));
        }
      });
    }
    catch(const std::runtime_error&)
    {
      return {};
    }

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
        reflex::match{
            [&]<typename T>(const T& values) -> value_type
              requires(
                  not std::same_as<std::decay_t<T>, std::string>
                  and requires {
                    values.size();
                    values[idx];
                  })
            {
              if(idx < values.size())
              {
                if constexpr(requires { value_type{values[idx]}; })
                {
                  return value_type{values[idx]};
                }
                else
                {
                  return values[idx];
                }
              }
              return {};
            },
            [](auto const&) -> value_type { return {}; },
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
        std::string name{current.lexeme};
        advance();

        // Resolve the leading dotted chain through context so aggregate members remain readable.
        while(at(token_kind::dot))
        {
          advance();
          auto member = consume(token_kind::identifier).lexeme;
          name.reserve(name.size() + member.size() + 1);
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
        reflex::match{
            // clang-format off
            []<typename T>(T const& value) -> value_type
              requires requires { static_cast<bool>(value); }
            { return static_cast<bool>(value); },
            []<typename T>(T const& value) -> value_type
              requires requires {{ value.empty() } -> std::same_as<bool>; }
            { return !value.empty(); },
            // clang-format on
            [](auto const&) -> value_type { return false; },
            },
            v);
  }

  static bool equal(const value_type& a, const value_type& b)
  {
    return std::visit(
        reflex::match{
            []<typename LHS, typename RHS>(LHS const& lhs, RHS const& rhs)
              requires requires { lhs == rhs; }
            { return lhs == rhs; },
            [](auto&&, auto&&) { return false; },
            },
            a,
            b);
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
// specialization to allow visiting std::optional<reflex::jinja::expr::loop_info>
template <> struct object_visitor<std::optional<reflex::jinja::expr::loop_info&>>
{
  template <typename Fn, typename Agg>
  static inline constexpr decltype(auto) operator()(Fn&& fn, std::string_view key, Agg&& agg)
  {
    if(agg.has_value())
    {
      object_visitor<reflex::jinja::expr::loop_info>{}(
          std::forward<Fn>(fn), key, std::forward<Agg>(agg).value());
    }
    else
    {
      std::forward<Fn>(fn)(poly::null);
    }
  }
};
} // namespace reflex::serde