#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <cmath>
#include <cstring>

#include <reflex/format.hpp>
#include <reflex/heapless/string.hpp>
#include <reflex/parse.hpp>
#include <reflex/serde.hpp>
#include <reflex/serde/json_value.hpp>
#endif

#include <reflex/serde/detail/escape.hpp>
#include <reflex/serde/detail/io.hpp>

REFLEX_EXPORT namespace reflex::serde::json
{
  namespace detail
  {
  template <typename var_type>
    requires(meta::is_template_instance_of(^^var_type, ^^poly::var))
  constexpr auto aggregate_types_of_var()
  {
    static constexpr auto types = []() {
      std::vector<std::meta::info> types;
      template for(constexpr auto t : var_type::infos::base_types)
      {
        using T = [:t:];
        if constexpr(aggregate_c<T>)
        {
          types.push_back(t);
        }
      }
      return define_static_array(types);
    }();
    return types;
  }
  template <typename var_type>
    requires(not meta::is_template_instance_of(^^var_type, ^^poly::var))
  constexpr auto aggregate_types_of_var()
  {
    static constexpr auto empty = std::array<std::meta::info, 0>{};
    return empty;
  }

  using escapes = serde::detail::json_escapes;

  constexpr bool needs_escape(char c)
  {
    return serde::detail::needs_escape<escapes, '"'>(c);
  }

  template <typename Ser> void write_escape(Ser& ser, char c)
  {
    serde::detail::write_escape<escapes, '"'>(ser, c);
  }

  template <typename Ser> void write_quoted(Ser& ser, std::string_view text)
  {
    serde::detail::write_backslash_quoted<escapes, '"'>(ser, text);
  }

  // The seven escapes JSON names, decoded, or -1 for anything else. Kept apart
  // from the \u case so it stays small enough to inline: folded together they
  // become an out-of-line call GCC declines to inline, on a path that runs once
  // per escape.
  constexpr int simple_escape(char esc)
  {
    switch(esc)
    {
      case '"':
        return '"';
      case '\\':
        return '\\';
      case '/':
        return '/';
      case 'b':
        return '\b';
      case 'f':
        return '\f';
      case 'n':
        return '\n';
      case 'r':
        return '\r';
      case 't':
        return '\t';
      default:
        return -1;
    }
  }

  // The cold half: \u, and every malformed escape. `next` yields the next body
  // byte and throws when there is none, `emit` takes the decoded byte. Both
  // cursors share this so the escape semantics exist in exactly one place.
  template <typename Next, typename Emit> void decode_rare_escape(char esc, Next&& next, Emit&& emit)
  {
    switch(esc)
    {
      case 'u':
      {
        // Only the subset below 0x80 is decoded, which is exactly what the
        // serializer emits: a control character, as one byte of UTF-8. Surrogate
        // pairs and higher code points would need a multi-byte encoding and
        // still throw.
        const int cp = serde::detail::decode_hex_escape(4, next, [](char d) {
          return std::format("Invalid \\u escape digit: {}", d);
        });
        if(cp > 0x7F)
        {
          throw std::runtime_error("\\uXXXX escapes not implemented");
        }
        emit(static_cast<char>(cp));
        return;
      }
      default:
        throw std::runtime_error(std::format("Unknown escape: \\{}", esc));
    }
  }

  // One escape, whichever kind. The common seven stay inline at the call site.
  template <typename Next, typename Emit> void decode_escape(Next&& next, Emit&& emit)
  {
    const char esc    = next();
    const int  simple = simple_escape(esc);
    if(simple >= 0)
    {
      emit(static_cast<char>(simple));
      return;
    }
    decode_rare_escape(esc, next, emit);
  }

  // What may legally follow a number: a structural character, or whitespace
  // before one.
  template <bool Comments = false> constexpr bool ends_number(char c) noexcept
  {
    return c == ',' or c == ']' or c == '}' or reflex::is_space(c) or (Comments and c == '/');
  }

  // `"name":` is a constant, so it is built once at compile time and promoted to
  // static storage.
  //
  // The name is checked here rather than escaped. Identifiers cannot need an
  // escape, but a serde::rename can carry an arbitrary string, and a key that
  // silently emitted an unescaped quote would produce invalid JSON.
  template <std::meta::info Member, bool Spaced = false> consteval std::string_view quoted_key()
  {
    constexpr std::string_view name = serialized_name(Member);
    static_assert(
        std::ranges::none_of(name, needs_escape),
        "a JSON object key cannot contain a quote, a backslash or a control "
        "character: check the serde::rename annotation on this member");
    std::string s;
    s.reserve(name.size() + 4);
    s += '"';
    s += name;
    s += Spaced ? "\": " : "\":";
    return {std::define_static_string(s), s.size()};
  }

  template <typename De, typename V> void load_object_member(De& de, V& v)
  {
    v = de.template load<std::remove_cvref_t<V>>();
    if constexpr(meta::is_template_instance_of(^^V, ^^poly::var))
    {
      if(v.is_object())
      {
        auto& obj = v.as_object();
        auto  type_key =
            std::ranges::find_if(obj, [](auto const& pair) { return pair.first == "__type"; });
        if(type_key != obj.end())
        {
          template for(constexpr auto a : detail::aggregate_types_of_var<V>())
          {
            if(type_key->second == std::hash<std::string_view>{}(identifier_of(dealias(decay(a)))))
            {
              using Agg = [:a:];
              Agg agg_value{};
              template for(constexpr auto member : define_static_array(
                               nonstatic_data_members_of(a, std::meta::access_context::current())))
              {
                constexpr std::string_view name = serialized_name(member);
                if(name == "__type")
                {
                  continue;
                }
                [[maybe_unused]] auto object_value_it = std::ranges::find_if(
                    obj, [name](auto const& pair) { return pair.first == name; });
                auto& member_value = agg_value.[:member:];
                visit(
                    [&]<typename U>([[maybe_unused]] U&& v) {
                      if constexpr(requires { member_value = std::forward<U>(v); })
                      {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wfloat-conversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
                        member_value = std::forward<U>(v);
#pragma GCC diagnostic pop
                      }
                      else
                      {
                        throw std::runtime_error("Cannot assign value to member");
                      }
                    },
                    std::move(object_value_it->second));
              }
              v = std::move(agg_value);
              break;
            }
          }
        }
      }
    }
  }

  template <typename Map, typename De> Map read_commented_object(De& de)
  {
    using node_type      = serde::comments_of<Map>;
    constexpr auto opts  = De::read_opts;
    auto&          cap   = de.comments_;
    node_type*     node  = cap.slot.template take<node_type>();
    const bool     root  = std::exchange(cap.root, false) and node != nullptr;
    bool           found = false;

    serde::comment_block  discarded;
    serde::comment_block* prev = nullptr;

    auto take_trailing = [&] {
      if(node == nullptr)
      {
        return;
      }
      de.take_trailing(prev != nullptr ? prev->trailing : serde::comment(*node).trailing);
    };
    auto close = [&] {
      take_trailing();
      if(node == nullptr)
      {
        de.drop_pending();
        return;
      }
      de.take_leading(serde::comment(*node).footer);
    };
    auto measure = [&] {
      if(root and cap.broke_line and serde::layout_of(*node).indent == 0)
      {
        serde::layout_of(*node).indent =
            static_cast<std::uint8_t>(std::min<std::size_t>(cap.line_column, 255));
        serde::layout_of(*node).indent_char = cap.indent_char;
      }
    };
    auto colon = [&] {
      de.ltrim();
      if(de.advance() != ':')
      {
        throw std::runtime_error("Expected ':' after object key");
      }
      de.ltrim();
      de.drop_pending();
    };

    de.skip_to_value();
    if(de.advance() != '{')
    {
      throw std::runtime_error("Expected '{' at start of JSON object");
    }
    de.ltrim();
    take_trailing();
    measure();

    Map value{};
    if(not de.at_end() and de.peek() == '}')
    {
      de.advance();
      close();
      return value;
    }

    while(true)
    {
      de.ltrim();
      measure();
      const std::string_view key = de.read_key();
      found                      = false;
      template for(constexpr auto member : define_static_array(
                       nonstatic_data_members_of(^^Map, std::meta::access_context::current())))
      {
        constexpr std::string_view name        = serialized_name(member);
        constexpr auto             node_member = serde::detail::comment_member_of<member>();
        using member_node = std::remove_cvref_t<typename[:type_of(node_member):]>;
        if(not found and key == name)
        {
          found = true;
          take_trailing();
          if(node != nullptr)
          {
            auto& block = serde::comment(node->[:node_member:]);
            de.take_leading(block.leading);
            prev = &block;
          }
          else
          {
            de.drop_pending();
          }
          colon();
          if constexpr(not std::same_as<member_node, serde::comment_block>)
          {
            if(node != nullptr)
            {
              cap.slot.give(node->[:node_member:]);
            }
          }
          load_object_member(de, value.[:member:]);
          cap.slot = {};
          de.drop_pending();
        }
      }
      if(not found)
      {
        if constexpr(opts.unknown_keys == serde::unknown_keys::skip)
        {
          take_trailing();
          de.drop_pending();
          prev = &discarded;
          colon();
          std::ignore = de.template load<json::value>();
          de.drop_pending();
        }
        else
        {
          throw std::runtime_error("Key not found in object");
        }
      }

      de.ltrim();
      const char sep = de.advance();
      if(sep == ',')
      {
        if constexpr(opts.trailing_commas)
        {
          de.ltrim();
          if(not de.at_end() and de.peek() == '}')
          {
            de.advance();
            close();
            break;
          }
        }
        continue;
      }
      if(sep == '}')
      {
        close();
        break;
      }
      throw std::runtime_error("Expected ',' or '}' in object");
    }
    return value;
  }

  struct indent_state
  {
    int depth = 0;
  };

  struct no_indent_state
  {};

  /** @brief The JSON reader's capture state.
   *
   * @p fresh_line makes the next skip start on a new line, at document start. @p broke_line tells
   * whether the last skip that consumed anything crossed a line break, @p indent_char is the
   * first byte of the indentation it measured. @p newline_last is true when the last byte it
   * consumed was a line break.
   */
  struct comment_capture : serde::detail::comment_capture
  {
    bool fresh_line   = false;
    bool broke_line   = false;
    bool newline_last = false;
    char indent_char  = ' ';
  };

  /** @brief The lines of the block comment body @p raw, one entry per line.
   *
   * Continuation lines lose their indentation and, when every non-empty one starts with `*`, that
   * decoration. An empty first or last line, the opening or closing line of a boxed comment, is
   * dropped.
   *
   * @return the lines, and whether the first one was kept, the one sharing a line with the
   * preceding token
   */
  inline std::pair<std::vector<std::string>, bool> split_block_comment(std::string_view raw)
  {
    std::vector<std::string_view> lines;
    for(auto part : raw | std::views::split('\n'))
    {
      lines.emplace_back(part.begin(), part.end());
    }
    bool decorated = false;
    for(std::size_t i = 1; i < lines.size(); ++i)
    {
      const auto start = lines[i].find_first_not_of(" \t");
      lines[i].remove_prefix(start == std::string_view::npos ? lines[i].size() : start);
    }
    for(std::size_t i = 1; i < lines.size(); ++i)
    {
      if(lines[i].empty())
      {
        continue;
      }
      decorated = lines[i].starts_with('*');
      if(not decorated)
      {
        break;
      }
    }
    if(decorated)
    {
      for(std::size_t i = 1; i < lines.size(); ++i)
      {
        if(not lines[i].empty())
        {
          lines[i].remove_prefix(1);
        }
      }
    }
    std::vector<std::string> out;
    for(auto line : lines)
    {
      out.emplace_back(serde::detail::comment_body(line));
    }
    bool first_kept = true;
    if(out.size() > 1 and out.back().empty())
    {
      out.pop_back();
    }
    if(out.size() > 1 and out.front().empty())
    {
      out.erase(out.begin());
      first_kept = false;
    }
    return {std::move(out), first_kept};
  }
  } // namespace detail

  /** @brief Output options of json::serializer, its second template argument. */
  struct options
  {
    int  indent   = 0;
    bool comments = false;

    constexpr bool operator==(options const&) const = default;
  };

  /** @brief Input options of json::deserializer, its second template argument. */
  struct read_options
  {
    bool                comments        = false;
    bool                trailing_commas = false;
    serde::unknown_keys unknown_keys    = serde::unknown_keys::error;

    constexpr bool operator==(read_options const&) const = default;
  };

  template <typename OutputIt, options Opts = options{}>
  class serializer : public serde::detail::serializer_base<OutputIt>
  {
  public:
    using serde::detail::serializer_base<OutputIt>::serializer_base;

    static constexpr options opts = Opts;

    static_assert(Opts.indent >= 0, "json::options::indent cannot be negative");
    static_assert(
        Opts.indent > 0 or not Opts.comments,
        "json::options: comments need indent > 0. A // comment runs to the end of its line, and "
        "indent 0 writes the whole document on one line");

    template <typename T, auto X>
      requires std::constructible_from<OutputIt, T&> and (std::constant_wrapper<X>::value == Opts)
    serializer(T& out, std::constant_wrapper<X>) : serde::detail::serializer_base<OutputIt>(out)
    {}

    static constexpr std::string_view format_name = "JSON";

    static constexpr std::string_view key_separator = Opts.indent > 0 ? ": " : ":";

    [[no_unique_address]] std::
        conditional_t<(Opts.indent > 0), detail::indent_state, detail::no_indent_state> indent_;

    /** @brief The width of one indent level. */
    constexpr int indent_width() const
    {
      return Opts.indent;
    }

    /** @brief Writes a line break and the indent of the current depth. */
    void newline()
      requires(Opts.indent > 0)
    {
      static constexpr std::string_view spaces = "                                                "
                                                 "                ";
      this->write_char('\n');
      auto n = static_cast<std::size_t>(indent_.depth * indent_width());
      while(n > 0)
      {
        const std::size_t run = std::min(n, spaces.size());
        this->write_raw(spaces.substr(0, run));
        n -= run;
      }
    }

    /** @brief One container being written: separators, line breaks and the depth it opened.
     *
     * The depth is entered at the first item and left at the closing token, or on unwinding, so
     * an empty container stays on one line and a throwing value leaves the depth right.
     */
    class nest
    {
      serializer* ser_   = nullptr;
      bool        first_ = true;

    public:
      nest()                       = default;
      nest(nest const&)            = delete;
      nest& operator=(nest const&) = delete;

      ~nest()
      {
        if constexpr(Opts.indent > 0)
        {
          if(ser_ != nullptr)
          {
            --ser_->indent_.depth;
          }
        }
      }

      /** @brief Writes what precedes the next item: a comma after the first, then its line. */
      void item(serializer& ser)
      {
        if(not first_)
        {
          ser.write_char(',');
        }
        first_ = false;
        if constexpr(Opts.indent > 0)
        {
          if(ser_ == nullptr)
          {
            ser_ = &ser;
            ++ser.indent_.depth;
          }
          ser.newline();
        }
      }

      /** @brief Writes the closing token @p close, on its own line when an item was written. */
      void close(serializer& ser, char close)
      {
        if constexpr(Opts.indent > 0)
        {
          if(ser_ != nullptr)
          {
            --ser.indent_.depth;
            ser_ = nullptr;
            ser.newline();
          }
        }
        ser.write_char(close);
      }
    };

    friend OutputIt tag_invoke(tag_t<serde::serialize>, serializer& ser, null_t const&)
    {
      ser.write_raw("null");
      return ser.out();
    }

    template <typename T>
    friend OutputIt
        tag_invoke(tag_t<serde::serialize>, serializer& ser, std::optional<T> const& value)
    {
      if(value.has_value())
      {
        serialize(ser, *value);
      }
      else
      {
        ser.write_raw("null");
      }
      return ser.out();
    }

    template <str_c Str>
    friend OutputIt tag_invoke(tag_t<serde::serialize>, serializer& ser, Str const& value)
    {
      if constexpr(meta::is_template_instance_of(^^Str, ^^std::array))
      {
        detail::write_quoted(
            ser, std::string_view{value.data(), ::strnlen(value.data(), value.size())});
      }
      else
      {
        detail::write_quoted(ser, std::string_view{value});
      }
      return ser.out();
    }

    template <number_c Num>
    friend OutputIt tag_invoke(tag_t<serde::serialize>, serializer& ser, Num const& value)
    {
      // The JSON grammar has no literal for a non-finite number, so emitting one
      // would produce a document no conforming reader accepts. Checked here and
      // not in write_digits, which the CSV and XML backends share and where
      // inf and nan do round-trip.
      if constexpr(std::floating_point<Num>)
      {
        if(not std::isfinite(value))
        {
          throw std::runtime_error("JSON has no literal for a non-finite number");
        }
      }
      serde::detail::write_digits(ser, value);
      return ser.out();
    }

    friend OutputIt tag_invoke(tag_t<serde::serialize>, serializer& ser, std::byte value)
    {
      serde::detail::write_digits(ser, std::to_integer<unsigned>(value));
      return ser.out();
    }

    template <std::same_as<char> Char>
    friend OutputIt tag_invoke(tag_t<serde::serialize>, serializer& ser, Char value)
    {
      ser.write_char('"');
      if(detail::needs_escape(value))
      {
        detail::write_escape(ser, value);
      }
      else
      {
        ser.write_char(value);
      }
      ser.write_char('"');
      return ser.out();
    }

    template <std::same_as<boolean> Boolean>
    friend OutputIt tag_invoke(tag_t<serde::serialize>, serializer& ser, Boolean value)
    {
      ser.write_raw(value ? "true" : "false");
      return ser.out();
    }

    template <derives_c<derive_t<Format>> T>
    friend OutputIt tag_invoke(tag_t<serde::serialize>, serializer& ser, T const& value)
    {
      // The rendered text has to be scanned before it reaches the sink, so it
      // cannot be formatted straight through. Anything a formatter is likely to
      // produce here fits the stack buffer.
      std::array<char, 128> buf{};
      const auto            r = std::format_to_n(buf.data(), buf.size(), "{}", value);
      if(static_cast<std::size_t>(r.size) <= buf.size())
      {
        detail::write_quoted(ser, std::string_view{buf.data(), static_cast<std::size_t>(r.size)});
      }
      else
      {
        detail::write_quoted(ser, std::format("{}", value));
      }
      return ser.out();
    }

    template <seq_c Seq>
    friend OutputIt tag_invoke(tag_t<serde::serialize>, serializer& ser, Seq const& value)
    {
      ser.write_char('[');
      if(value.empty())
      {
        ser.write_char(']');
        return ser.out();
      }

      if constexpr(Opts.indent > 0)
      {
        nest items;
        for(const auto& elem : value)
        {
          items.item(ser);
          serialize(ser, elem);
        }
        items.close(ser, ']');
        return ser.out();
      }

      auto view = std::views::all(value);

      for(const auto& elem : view | std::views::take(value.size() - 1))
      {
        serialize(ser, elem);
        ser.write_char(',');
      }
      serialize(ser, view.back());

      ser.write_char(']');
      return ser.out();
    }

    template <pair_c Pair>
    friend OutputIt tag_invoke(tag_t<serde::serialize>, serializer& ser, Pair const& value)
    {
      ser.write_char('{');
      nest entry;
      entry.item(ser);
      serialize(ser, value.first);
      ser.write_raw(key_separator);
      reflex::visit([&ser](const auto& v) { serialize(ser, v); }, value.second);
      entry.close(ser, '}');
      return ser.out();
    }

    template <map_c Map>
    friend OutputIt tag_invoke(tag_t<serde::serialize>, serializer& ser, Map const& value)
    {
      ser.write_char('{');

      nest entries;

      for(const auto& [key, val] : value)
      {
        entries.item(ser);
        serialize(ser, key);
        ser.write_raw(key_separator);
        reflex::visit(
            [&ser]<typename U>(const U& v) {
              using var_type = typename Map::mapped_type;
              if constexpr(std::ranges::contains(detail::aggregate_types_of_var<var_type>(), ^^U))
              {
                serialize(ser, v, std::cw<true>);
              }
              else
              {
                serialize(ser, v);
              }
            },
            val);
      }
      entries.close(ser, '}');
      return ser.out();
    }

    template <visitable_c T>
    friend OutputIt tag_invoke(tag_t<serde::serialize>, serializer& ser, T const& value)
    {
      return visit([&ser](const auto& v) { return serialize(ser, v); }, value);
    }
  };

  template <typename... TArgs>
  serializer(std::basic_string<TArgs...> & out)
      -> serializer<std::back_insert_iterator<std::basic_string<TArgs...>>>;
  serializer(std::ofstream & out) -> serializer<std::ostreambuf_iterator<char>>;
  serializer(std::ostringstream & out) -> serializer<std::ostreambuf_iterator<char>>;
  REFLEX_SERDE_TAGGED_SERIALIZER_DEDUCTION_GUIDES(serializer);

  template <typename OutputIt, options Opts, aggregate_c Agg, typename TagT = std::false_type>
    requires(not(str_c<Agg> or seq_c<Agg>)) // std::array<char> may be considered as an aggregate
  OutputIt tag_invoke(
      tag_default_t<serde::serialize>, serializer<OutputIt, Opts> & ser, Agg const& value,
      TagT = {})
  {
    ser.write_char('{');

    constexpr bool tag = TagT::value;

    typename serializer<OutputIt, Opts>::nest members;

    if constexpr(tag)
    {
      members.item(ser);
      ser.write_raw("\"__type\"");
      ser.write_raw(ser.key_separator);
      // Hashing at compile time would change the emitted id, which is a wire format
      // break.
      static auto expected_id = std::hash<std::string_view>{}(identifier_of(dealias(decay(^^Agg))));
      serialize(ser, expected_id);
    }

    static constexpr auto type = decay(type_of(^^value));
    template for(constexpr auto member : define_static_array(
                     nonstatic_data_members_of(type, std::meta::access_context::current())))
    {
      constexpr bool omit         = serde::omits_when_empty(member);
      auto const&    member_value = value.[:member:];
      if constexpr(omit)
      {
        if(serde::is_empty_value(member_value))
        {
          continue;
        }
      }
      members.item(ser);
      static constexpr std::string_view key_token = detail::quoted_key<member, (Opts.indent > 0)>();
      ser.write_raw(key_token);
      reflex::visit([&ser](const auto& v) { serialize(ser, v); }, member_value);
    }
    members.close(ser, '}');
    return ser.out();
  }

  template <std::input_iterator InputIt, read_options Opts = read_options{}>
  class deserializer : public serde::detail::subrange_deserializer<InputIt>
  {
    using base = serde::detail::subrange_deserializer<InputIt>;
    using base::cursor_;

    // Backs read_key() when the key cannot be borrowed. One buffer, not a pool:
    // a key is consumed by object_visit and dead before the next key is read.
    std::string key_buf_;

  public:
    using base::at_end;
    using base::base;

    static constexpr read_options read_opts = Opts;

    [[no_unique_address]] std::
        conditional_t<Opts.comments, detail::comment_capture, serde::detail::no_comment_state>
            comments_;

    template <typename T, auto X>
      requires std::constructible_from<base, T> and (std::constant_wrapper<X>::value == Opts)
    deserializer(T&& in, std::constant_wrapper<X>) : base(std::forward<T>(in))
    {}

    /** @brief Reads the document header into @p node's leading lines, called by load_commented. */
    template <typename Node>
    void begin_commented(Node& node)
      requires(Opts.comments)
    {
      comments_.root       = true;
      comments_.fresh_line = true;
      skip_space_and_comments();
      take_leading(serde::comment(node).leading);
    }

    /** @brief Reads the document footer into @p node's footer and records the final line break. */
    template <typename Node>
    void end_commented(Node& node)
      requires(Opts.comments)
    {
      drop_pending();
      skip_space_and_comments();
      take_leading(serde::comment(node).footer);
      if constexpr(serde::detail::comment_tree_c<Node>)
      {
        if constexpr(base::bulk_scan)
        {
          serde::layout_of(node).final_newline = this->input().ends_with('\n');
        }
        else
        {
          serde::layout_of(node).final_newline = comments_.newline_last;
        }
      }
    }

    /** @brief Moves the pending comments read on the previous token's line into @p trailing,
     * joined by one space and after one space when @p trailing is not empty.
     */
    void take_trailing(std::string& trailing)
      requires(Opts.comments)
    {
      auto&       pending = comments_.pending;
      std::size_t n       = 0;
      for(; n < pending.size() and pending[n].same_line; ++n)
      {
        if(not trailing.empty())
        {
          trailing += ' ';
        }
        trailing += pending[n].text;
      }
      pending.erase(pending.begin(), pending.begin() + static_cast<std::ptrdiff_t>(n));
    }

    /** @brief Moves every pending comment and blank line into @p lines. */
    void take_leading(std::vector<std::string>& lines)
      requires(Opts.comments)
    {
      for(auto& entry : comments_.pending)
      {
        lines.push_back(std::move(entry.text));
      }
      comments_.pending.clear();
    }

    /** @brief Drops every pending comment. */
    void drop_pending()
      requires(Opts.comments)
    {
      comments_.pending.clear();
    }

    template <auto X>
      requires(std::constant_wrapper<X>::value == Opts)
    deserializer(InputIt first, InputIt last, std::constant_wrapper<X>) : base(first, last)
    {}

    InputIt begin() const
    {
      return cursor_.begin();
    }

    InputIt end() const
    {
      return cursor_.end();
    }

    auto peek() const
    {
      if(at_end())
      {
        throw std::runtime_error("Unexpected end of JSON input");
      }
      return *cursor_.begin();
    }

    auto advance()
    {
      const auto c = peek();
      cursor_.advance(1);
      return c;
    }

    void advance_to(InputIt pos)
    {
      cursor_ = {pos, cursor_.end()};
    }

    void expect(std::string_view token)
    {
      for(char expected : token)
      {
        if(advance() != expected)
        {
          throw std::runtime_error(std::format("Expected '{}'", token));
        }
      }
    }

    // reflex::is_space accepts 0x09 to 0x0D and 0x20, so it also takes VT and
    // FF, which JSON's grammar does not. This parser has always accepted them.
    // Narrowing to " \t\n\r" would change what parses.
    void ltrim()
    {
      if constexpr(Opts.comments)
      {
        skip_space_and_comments();
      }
      else
      {
        while(not at_end() and reflex::is_space(*cursor_.begin()))
        {
          cursor_.advance(1);
        }
      }
    }

    /** @brief Skips what may precede a value: nothing, or whitespace and comments when the
     * read options accept comments, so a value opening the document may follow a comment.
     */
    void skip_to_value()
    {
      if constexpr(Opts.comments)
      {
        skip_space_and_comments();
      }
    }

    /** @brief Skips whitespace, line comments and block comments.
     *
     * While load_commented runs, each comment and each blank line is queued as pending, with
     * whether it sits on the line of the preceding token, and the indentation of the line the
     * next token starts is measured.
     */
    void skip_space_and_comments()
      requires(Opts.comments)
    {
      auto&       cap       = comments_;
      const bool  capture   = cap.active;
      bool        crossed   = cap.fresh_line;
      bool        same_line = not crossed;
      bool        blank     = crossed;
      bool        consumed  = false;
      std::size_t column    = 0;
      char        fill      = ' ';
      std::string text;
      cap.fresh_line   = false;
      cap.newline_last = false;
      while(not at_end())
      {
        const char c = *cursor_.begin();
        if(c == '\n')
        {
          if(capture and crossed and blank)
          {
            cap.pending.push_back({std::string{serde::blank_line}, 0, false});
          }
          crossed   = true;
          same_line = false;
          blank     = true;
          column    = 0;
          cursor_.advance(1);
          consumed         = true;
          cap.newline_last = true;
          continue;
        }
        if(reflex::is_space(c))
        {
          if(blank)
          {
            fill = column == 0 ? c : fill;
            ++column;
          }
          cursor_.advance(1);
          consumed         = true;
          cap.newline_last = false;
          continue;
        }
        if(c != '/')
        {
          break;
        }
        cursor_.advance(1);
        consumed         = true;
        cap.newline_last = false;
        const char kind  = at_end() ? '\0' : *cursor_.begin();
        if(kind == '/')
        {
          cursor_.advance(1);
          read_line_comment(capture ? &text : nullptr);
          if(capture)
          {
            cap.pending.push_back(
                {std::string{serde::detail::comment_body(text)}, column, same_line});
          }
          blank = false;
        }
        else if(kind == '*')
        {
          cursor_.advance(1);
          const bool multi_line = read_block_comment(capture ? &text : nullptr);
          if(capture)
          {
            auto [lines, first_kept] = detail::split_block_comment(text);
            bool on_line             = same_line and first_kept;
            for(auto& line : lines)
            {
              cap.pending.push_back({std::move(line), column, on_line});
              on_line = false;
            }
          }
          if(multi_line)
          {
            crossed   = true;
            same_line = false;
          }
          blank = false;
        }
        else
        {
          throw std::runtime_error("Unexpected '/' in JSON");
        }
      }
      if(consumed)
      {
        cap.broke_line = crossed;
        if(crossed and blank)
        {
          cap.line_column = column;
          cap.indent_char = fill;
        }
      }
    }

  private:
    void read_line_comment(std::string* text)
    {
      if constexpr(base::bulk_scan)
      {
        const std::string_view rest = this->rest();
        const std::size_t      end  = std::min(rest.find('\n'), rest.size());
        if(text != nullptr)
        {
          text->assign(rest.substr(0, end));
        }
        this->skip(end);
      }
      else
      {
        if(text != nullptr)
        {
          text->clear();
        }
        while(not at_end() and *cursor_.begin() != '\n')
        {
          if(text != nullptr)
          {
            text->push_back(*cursor_.begin());
          }
          cursor_.advance(1);
        }
      }
    }

    bool read_block_comment(std::string* text)
    {
      if constexpr(base::bulk_scan)
      {
        const std::string_view rest = this->rest();
        const std::size_t      end  = rest.find("*/");
        if(end == std::string_view::npos)
        {
          throw std::runtime_error("Unterminated JSON comment");
        }
        const std::string_view body = rest.substr(0, end);
        if(text != nullptr)
        {
          text->assign(body);
        }
        this->skip(end + 2);
        return body.find('\n') != std::string_view::npos;
      }
      else
      {
        if(text != nullptr)
        {
          text->clear();
        }
        bool multi_line = false;
        bool star       = false;
        while(not at_end())
        {
          const char c = *cursor_.begin();
          cursor_.advance(1);
          if(star and c == '/')
          {
            if(text != nullptr)
            {
              text->pop_back();
            }
            return multi_line;
          }
          star       = c == '*';
          multi_line = multi_line or c == '\n';
          if(text != nullptr)
          {
            text->push_back(c);
          }
        }
        throw std::runtime_error("Unterminated JSON comment");
      }
    }

  public:
    // Reads an object key and hands back a view of it.
    //
    // The view points into the input buffer, not into anything the parse owns,
    // so advancing the cursor does not invalidate it. A key borrowed here never
    // reaches the caller: object_visit copies what it needs into the target, and
    // the fallback below parks a decoded key in key_buf_, which the next key
    // overwrites.
    //
    // Values are a different matter. A std::string_view member does hand a borrow
    // to the caller, deliberately, and carries the input-lifetime contract with
    // it. That is the string reader's business, not this one's, and it is why
    // the fallback here can keep using a buffer the next call reuses.
    //
    // The fast path is a key with no escape on a contiguous cursor, which is
    // every key in practice. It borrows. Everything else, an escaped key or a
    // non-contiguous cursor, goes through the ordinary string reader and lands
    // in key_buf_, so the escape semantics are not duplicated here.
    std::string_view read_key()
    {
      if constexpr(base::bulk_scan)
      {
        const std::string_view sv = this->rest();
        if(sv.size() >= 2 and sv.front() == '"')
        {
          // With no backslash in the body, the first quote after the opening one
          // is necessarily the terminator, so the escaped-quote walk is not
          // needed. Both scans are bounded by the key.
          const std::size_t end = sv.find('"', 1);
          if(end != std::string_view::npos)
          {
            const std::string_view span = sv.substr(1, end - 1);
            if(span.find('\\') == std::string_view::npos)
            {
              this->skip(end + 1);
              return span;
            }
          }
        }
      }
      key_buf_ = this->template load<std::string>();
      return key_buf_;
    }

    // makes load() without an explicit type read a json::value
    using default_load_type = json::value;

    friend auto tag_invoke(tag_t<serde::deserialize>, deserializer& de)
    {
      return deserialize(de, std::type_identity<json::value>{});
    }

    friend auto
        tag_invoke(tag_t<serde::deserialize>, deserializer& de, std::type_identity<json::null_t>)
    {
      de.skip_to_value();
      de.expect("null");
      return null;
    }

    template <typename T>
    friend std::optional<T> tag_invoke(
        tag_t<serde::deserialize>,
        deserializer& de,
        std::type_identity<std::optional<T>>)
    {
      de.skip_to_value();
      if(de.peek() == 'n')
      {
        de.expect("null");
        return std::nullopt;
      }
      else
      {
        return de.template load<T>();
      }
    }

    // Offset of the closing quote of the string body starting at `sv[0]`, having
    // skipped over any quote that is itself escaped.
    //
    // Linear, and it has to be argued because the obvious spelling is not. Each
    // resumed find() starts strictly after the previous candidate, so the
    // forward scans are disjoint. The backwards run counts are disjoint too:
    // two candidate quotes cannot share a backslash run, the earlier quote sits
    // between them. So the whole thing is bounded by the string length, not by
    // the length times the number of escapes.
    static std::size_t find_terminator(std::string_view sv)
    {
      std::size_t end = sv.find('"');
      while(end != std::string_view::npos)
      {
        std::size_t run = end;
        while(run > 0 and sv[run - 1] == '\\')
        {
          --run;
        }
        if(((end - run) % 2) == 0)
        {
          return end;
        }
        end = sv.find('"', end + 1);
      }
      throw std::runtime_error("Unterminated JSON string");
    }

    // Borrowed read. A std::string_view destination is handed a view of the input
    // rather than a copy, and choosing that member type is the opt-in.
    //
    // LIFETIME: the view is valid only while the input this deserializer was given
    // stays alive and unmodified. Nothing in the type system enforces that, so
    // `json::deserializer{std::string{...}}.load<T>()` leaves every borrowed
    // member dangling. Deserialize from an lvalue that outlives the result, or
    // from serde::mmap_input_stream.
    //
    // An escaped value has no borrowed form: the decoded bytes are not a run of
    // the input, and there is nowhere to put them that outlives the parse. It
    // throws. Whether a value carries an escape is the producer's choice rather
    // than the schema's, so this is the caller's risk to take, deliberately.
    //
    // The escape is found before a single byte is emitted, not partway through:
    // find_terminator bounds the body first, then one scan decides. So there is
    // no partial write to unwind, and the throw leaves the cursor untouched.
    template <str_c Str>
      requires serde::detail::borrowed_string_sink_c<Str> and deserializer::bulk_scan
    friend Str tag_invoke(tag_t<serde::deserialize>, deserializer& de, std::type_identity<Str>)
    {
      de.skip_to_value();
      if(de.advance() != '"')
      {
        throw std::runtime_error("Expected '\"' at start of JSON string");
      }

      const std::string_view sv   = de.rest();
      const std::string_view span = sv.substr(0, find_terminator(sv));
      if(span.find('\\') != std::string_view::npos)
      {
        throw std::runtime_error(
            "JSON: a borrowed string destination cannot hold an escaped value");
      }
      de.skip(span.size() + 1);
      return Str{span};
    }

    // A string destination that neither owns the storage the decoded bytes would
    // go into nor can be pointed at the input. Two shapes reach here: a type that
    // owns nothing and cannot be built from a run, char const* being one, and a
    // std::string_view on the streaming cursor, where there is no buffer to point
    // at because the input arrives a character at a time.
    //
    // Refused here rather than left to the fill path below, which is written
    // against iterators and so accepts a view syntactically, then fails on the
    // assignment with a message naming neither the type nor the member.
    //
    // A separate overload rather than a static_assert inside the good one: the
    // assert alone does not stop the body from instantiating, so the original
    // error still followed it. It returns Str rather than auto for the same
    // reason, so the caller does not then report a void conversion on top.
    template <str_c Str>
      requires(not serde::detail::string_sink_c<Str>)
          and (not(serde::detail::borrowed_string_sink_c<Str> and deserializer::bulk_scan))
    friend Str tag_invoke(tag_t<serde::deserialize>, deserializer& de, std::type_identity<Str>)
    {
      if constexpr(serde::detail::borrowed_string_sink_c<Str>)
      {
        static_assert(
            false,
            std::string(display_string_of(dealias(^^Str)))
                + " cannot be a JSON string destination on this cursor: a borrowed read needs a"
                  " contiguous input to point at, and this deserializer reads a character at a"
                  " time (use std::string, or deserialize from a contiguous input)");
      }
      else
      {
        static_assert(
            false,
            std::string(display_string_of(dealias(^^Str)))
                + " cannot be a JSON string destination: it does not own writable storage"
                  " (expected std::string, reflex::heapless::string<N> or std::array<char, N>)");
      }
      std::unreachable();
    }

    template <str_c Str>
      requires serde::detail::string_sink_c<Str>
    friend auto tag_invoke(tag_t<serde::deserialize>, deserializer& de, std::type_identity<Str>)
    {
      de.skip_to_value();
      if(de.advance() != '"')
      {
        throw std::runtime_error("Expected '\"' at start of JSON string");
      }

      Str value{};

      auto push = [&value] {
        if constexpr(serde::detail::growable_string_sink_c<Str>)
        {
          return [&value](char c) { value.push_back(c); };
        }
        else
        {
          return [it = std::begin(value), end = std::end(value)](char c) mutable {
            if(it == end)
            {
              throw std::out_of_range("String too long to fit in target type");
            }
            *it++ = c;
          };
        }
      }();

      // Appends a whole run. std::string takes it as one memcpy, which is the
      // point of the bulk path. A fixed-capacity target has no bulk append and
      // falls back to the per-character push, which keeps its bounds check: an
      // up-front check against the encoded length would wrongly reject a string
      // whose *decoded* length fits, since an escape is 2 or 6 bytes in and 1
      // byte out.
      auto append_run = [&value, &push](std::string_view run) {
        if constexpr(requires { value.append(std::string_view{}); })
        {
          value.append(run);
        }
        else
        {
          for(const char c : run)
          {
            push(c);
          }
        }
      };

      if constexpr(deserializer::bulk_scan)
      {
        const std::string_view sv = de.rest();

        // The bound, located once. Everything below is confined to it, so no
        // scan can run off into the rest of the document.
        const std::size_t      end  = find_terminator(sv);
        const std::string_view span = sv.substr(0, end);

        // The overwhelmingly common case: no escape at all, so the body is one
        // contiguous run and goes across in one append.
        std::size_t n = span.find('\\');
        if(n == std::string_view::npos)
        {
          append_run(span);
          de.skip(end + 1);
          return value;
        }

        std::size_t start = 0;
        while(true)
        {
          append_run(span.substr(start, n - start));

          std::size_t i    = n + 1;
          auto        next = [&span, &i]() -> char {
            if(i >= span.size())
            {
              throw std::runtime_error("Unterminated JSON string");
            }
            return span[i++];
          };
          const char esc    = next();
          const int  simple = detail::simple_escape(esc);
          if(simple >= 0)
          {
            push(static_cast<char>(simple));
          }
          else
          {
            detail::decode_rare_escape(esc, next, push);
          }
          start = i;

          n = span.find('\\', start);
          if(n == std::string_view::npos)
          {
            append_run(span.substr(start));
            break;
          }
        }
        de.skip(end + 1);
        return value;
      }
      else
      {
        while(not de.at_end())
        {
          const char c = de.advance();
          if(c == '"')
          {
            return value;
          }

          if(c == '\\')
          {
            const char esc    = de.advance();
            const int  simple = detail::simple_escape(esc);
            if(simple >= 0)
            {
              push(static_cast<char>(simple));
            }
            else
            {
              detail::decode_rare_escape(esc, [&de]() -> char { return de.advance(); }, push);
            }
          }
          else
          {
            push(c);
          }
        }
        throw std::runtime_error("Unterminated JSON string");
      }
    }

    template <number_c Num>
    friend auto tag_invoke(tag_t<serde::deserialize>, deserializer& de, std::type_identity<Num>)
    {
      de.skip_to_value();
      Num value;
      if constexpr(deserializer::bulk_scan)
      {
        const auto first = std::to_address(de.begin());
        const auto last  = std::to_address(de.end());
        auto [ptr, ec]   = std::from_chars(first, last, value);
        // A number is followed by more document, so the end of the input is not
        // the bound: what must follow is a structural character or whitespace.
        // Anything else, `1.2.3` being the case that used to slip through, is
        // trailing garbage and is rejected here rather than left for the next
        // token to fail on with an unrelated message.
        if(ec != std::errc{} or (ptr != last and not detail::ends_number<Opts.comments>(*ptr)))
        {
          throw std::runtime_error("Failed to parse number");
        }
        const auto offset = ptr - first;
        de.advance_to(std::next(de.begin(), offset));
      }
      else
      {
        heapless::string<64> token{};
        while(not de.at_end())
        {
          const auto c = de.peek();
          if((c == '-') or (c == '+') or (c == '.') or (c == 'e') or (c == 'E') or is_digit(c))
          {
            token.push_back(c);
            de.advance();
          }
          else
          {
            break;
          }
        }
        // Two checks, matching the contiguous path's single one. The token must
        // be consumed whole, which covers `1.2.3` where the collector is more
        // permissive than the parser, and what stopped the collector must be a
        // character that may follow a number, which covers `1.5abc`.
        auto [ptr, ec] = std::from_chars(token.data(), token.data() + token.size(), value);
        if(ec
           != std::errc{}
           or ptr
           != token.data()
           + token.size()
           or (not de.at_end() and not detail::ends_number<Opts.comments>(de.peek())))
        {
          throw std::runtime_error("Failed to parse number");
        }
      }
      return value;
    }

    friend auto tag_invoke(tag_t<serde::deserialize>, deserializer& de, std::type_identity<boolean>)
    {
      de.skip_to_value();
      if(de.peek() == 't')
      {
        de.expect("true");
        return true;
      }
      if(de.peek() == 'f')
      {
        de.expect("false");
        return false;
      }
      throw std::runtime_error("Expected 'true' or 'false'");
    }

    template <derives_c<derive_t<Parse>> T>
    friend auto tag_invoke(tag_t<serde::deserialize>, deserializer& de, std::type_identity<T>)
    {
      auto token  = de.template load<heapless::string<64>>();
      auto result = parse<std::remove_cvref_t<T>>(token);
      if(!result)
      {
        throw std::runtime_error(
            std::format(
                "Failed to parse value: {}", std::generic_category().message(int(result.error()))));
      }
      return std::move(result).value();
    }

    template <seq_c Seq>
    friend auto tag_invoke(tag_t<serde::deserialize>, deserializer& de, std::type_identity<Seq>)
    {
      de.skip_to_value();
      if(de.advance() != '[')
        throw std::runtime_error("Expected '[' at start of JSON array");

      Seq value{};

      de.ltrim();
      if(not de.at_end() and de.peek() == ']')
      {
        de.advance();
        return value;
      }

      using elem_type = typename std::remove_cvref_t<decltype(value)>::value_type;

      auto push = serde::detail::make_pusher(
          value, "Array has more elements than target type can hold");

      while(true)
      {
        push(de.template load<elem_type>());

        de.ltrim();
        const char sep = de.advance();
        if(sep == ',')
        {
          de.ltrim();
          if constexpr(Opts.trailing_commas)
          {
            if(not de.at_end() and de.peek() == ']')
            {
              de.advance();
              break;
            }
          }
          continue;
        }
        if(sep == ']')
        {
          break;
        }
        throw std::runtime_error("Expected ',' or ']' in array");
      }
      return value;
    }

    template <typename var_type>
      requires(meta::is_template_instance_of(^^var_type, ^^poly::var))
    friend auto
        tag_invoke(tag_t<serde::deserialize>, deserializer& de, std::type_identity<var_type>)
            -> var_type
    {
      de.ltrim();
      switch(de.peek())
      {
        case 't':
        case 'f':
          return de.template load<boolean>();
        case 'n':
          return de.template load<null_t>();
        case '{':
          return de.template load<typename var_type::obj_type>();
        case '[':
          return de.template load<typename var_type::arr_type>();
        case '"':
          return de.template load<json::string>();
        default:
          return de.template load<json::number>();
      }
    }
  };

  REFLEX_SERDE_DESERIALIZER_DEDUCTION_GUIDES(deserializer);
  REFLEX_SERDE_TAGGED_DESERIALIZER_DEDUCTION_GUIDES(deserializer);

  template <typename InputIt, read_options Opts, object_visitable_c Map>
    requires(
        not(meta::is_template_instance_of(^^Map, ^^poly::var)
            // std::array<char> may be considered as a visitable object
            or str_c<Map>
            or seq_c<Map>))
  auto tag_invoke(
      tag_default_t<serde::deserialize>, deserializer<InputIt, Opts> & de, std::type_identity<Map>)
  {
    if constexpr(Opts.comments and serde::detail::reflected_aggregate_c<Map>)
    {
      if(de.comments_.active)
      {
        return detail::read_commented_object<Map>(de);
      }
    }
    de.skip_to_value();
    if(de.advance() != '{')
      throw std::runtime_error("Expected '{' at start of JSON object");

    de.ltrim();
    if(!de.at_end() and de.peek() == '}')
    {
      de.advance();
      return Map{};
    }

    Map value{};
    while(true)
    {
      de.ltrim();
      const std::string_view key = de.read_key();
      de.ltrim();
      if(de.advance() != ':')
      {
        throw std::runtime_error("Expected ':' after object key");
      }
      de.ltrim();

      auto load_member = [&de]<typename V>(V& v) { detail::load_object_member(de, v); };
      if constexpr(Opts.unknown_keys == serde::unknown_keys::skip)
      {
        if(not serde::object_try_visit_flat(key, value, load_member))
        {
          std::ignore = de.template load<json::value>();
        }
      }
      else
      {
        serde::object_visit_flat(key, value, load_member);
      }

      de.ltrim();
      const char sep = de.advance();
      if(sep == ',')
      {
        if constexpr(Opts.trailing_commas)
        {
          de.ltrim();
          if(not de.at_end() and de.peek() == '}')
          {
            de.advance();
            break;
          }
        }
        continue;
      }
      if(sep == '}')
      {
        break;
      }
      throw std::runtime_error("Expected ',' or '}' in object");
    }
    return value;
  }

} // namespace reflex::serde::json

REFLEX_EXPORT namespace reflex::serde::ser
{
  constexpr auto json = ^^reflex::serde::json::serializer;
}

REFLEX_EXPORT namespace reflex::serde::de
{
  constexpr auto json = ^^reflex::serde::json::deserializer;
}
