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
        if constexpr(aggregate_c<T> and dealias(t) != dealias(^^null_t))
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

  // One entry per byte value, true when the byte cannot appear literally inside a
  // JSON string: 0x00-0x1F, '"' and '\\'. '/' is deliberately absent, JSON allows
  // it to be escaped but does not require it. Bytes 0x80-0xFF are absent too, they
  // are UTF-8 lead and continuation bytes and pass through unchanged.
  inline constexpr std::array<bool, 256> escape_table = [] {
    std::array<bool, 256> t{};
    for(unsigned c = 0; c < 0x20; ++c)
    {
      t[c] = true;
    }
    t[static_cast<unsigned char>('"')]  = true;
    t[static_cast<unsigned char>('\\')] = true;
    return t;
  }();

  constexpr bool needs_escape(char c)
  {
    return escape_table[static_cast<unsigned char>(c)];
  }

  // A bulk scan is wrong here. JSON's escape set is thirty-four bytes, so a min
  // over find(char) is thirty-four memchr passes and find_first_of is a per-byte
  // loop rescanning all thirty-four. The table walk beats both.
  inline std::size_t find_escapable(std::string_view s)
  {
    const auto* const first = s.data();
    const auto* const last  = first + s.size();
    const auto*       it    = std::find_if(first, last, needs_escape);
    return it == last ? std::string_view::npos : static_cast<std::size_t>(it - first);
  }

  // Emits the two-character escape for the seven bytes JSON names, and \u00XX for
  // every other control character.
  template <typename Ser> void write_escape(Ser& ser, char c)
  {
    switch(c)
    {
      case '"':
        ser.write_raw("\\\"");
        return;
      case '\\':
        ser.write_raw("\\\\");
        return;
      case '\b':
        ser.write_raw("\\b");
        return;
      case '\f':
        ser.write_raw("\\f");
        return;
      case '\n':
        ser.write_raw("\\n");
        return;
      case '\r':
        ser.write_raw("\\r");
        return;
      case '\t':
        ser.write_raw("\\t");
        return;
      default:
      {
        static constexpr std::string_view hex = "0123456789abcdef";
        const auto                        u   = static_cast<unsigned char>(c);
        const char buf[6] = {'\\', 'u', '0', '0', hex[u >> 4], hex[u & 0x0F]};
        ser.write_raw(std::string_view{buf, sizeof(buf)});
        return;
      }
    }
  }

  // Writes the body of a JSON string: runs of clean bytes go out whole, only the
  // bytes needing an escape are handled one at a time.
  template <typename Ser> void write_escaped(Ser& ser, std::string_view text)
  {
    serde::detail::write_with_escapes(
        ser,
        text,
        [](std::string_view s, std::size_t pos) {
          const std::size_t n = find_escapable(s.substr(pos));
          return n == std::string_view::npos ? n : pos + n;
        },
        [](Ser& out, char c) { write_escape(out, c); });
  }

  template <typename Ser> void write_quoted(Ser& ser, std::string_view text)
  {
    ser.write_char('"');
    write_escaped(ser, text);
    ser.write_char('"');
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
        int cp = 0;
        for(int i = 0; i < 4; ++i)
        {
          const char d = next();
          int        n = -1;
          if(d >= '0' and d <= '9')
          {
            n = d - '0';
          }
          else if(d >= 'a' and d <= 'f')
          {
            n = d - 'a' + 10;
          }
          else if(d >= 'A' and d <= 'F')
          {
            n = d - 'A' + 10;
          }
          else
          {
            throw std::runtime_error(std::format("Invalid \\u escape digit: {}", d));
          }
          cp = (cp << 4) | n;
        }
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
  constexpr bool ends_number(char c) noexcept
  {
    return c == ',' or c == ']' or c == '}' or reflex::is_space(c);
  }

  // `"name":` is a constant, so it is built once at compile time and promoted to
  // static storage.
  //
  // The name is checked here rather than escaped. Identifiers cannot need an
  // escape, but a serde::rename can carry an arbitrary string, and a key that
  // silently emitted an unescaped quote would produce invalid JSON.
  template <std::meta::info Member> consteval std::string_view quoted_key()
  {
    constexpr std::string_view name = serialized_name(Member);
    static_assert(
        std::ranges::none_of(name, needs_escape),
        "a JSON object key cannot contain a quote, a backslash or a control "
        "character: check the serde::rename annotation on this member");
    std::string s;
    s.reserve(name.size() + 3);
    s += '"';
    s += name;
    s += "\":";
    return {std::define_static_string(s), s.size()};
  }
  } // namespace detail

  template <typename OutputIt> class serializer : public serde::detail::serializer_base<OutputIt>
  {
  public:
    using serde::detail::serializer_base<OutputIt>::serializer_base;

    static constexpr std::string_view format_name = "JSON";

    friend OutputIt tag_invoke(tag_t<serde::serialize>, serializer<OutputIt>& ser, null_t const&)
    {
      ser.write_raw("null");
      return ser.out();
    }

    template <typename T>
    friend OutputIt tag_invoke(
        tag_t<serde::serialize>,
        serializer<OutputIt>&   ser,
        std::optional<T> const& value)
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
    friend OutputIt tag_invoke(tag_t<serde::serialize>, serializer<OutputIt>& ser, Str const& value)
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
    friend OutputIt tag_invoke(tag_t<serde::serialize>, serializer<OutputIt>& ser, Num const& value)
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

    template <std::same_as<char> Char>
    friend OutputIt tag_invoke(tag_t<serde::serialize>, serializer<OutputIt>& ser, Char value)
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
    friend OutputIt tag_invoke(tag_t<serde::serialize>, serializer<OutputIt>& ser, Boolean value)
    {
      ser.write_raw(value ? "true" : "false");
      return ser.out();
    }

    template <derives_c<derive_t<Format>> T>
    friend OutputIt tag_invoke(tag_t<serde::serialize>, serializer<OutputIt>& ser, T const& value)
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
    friend OutputIt tag_invoke(tag_t<serde::serialize>, serializer<OutputIt>& ser, Seq const& value)
    {
      ser.write_char('[');
      if(value.empty())
      {
        ser.write_char(']');
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
    friend OutputIt
        tag_invoke(tag_t<serde::serialize>, serializer<OutputIt>& ser, Pair const& value)
    {
      ser.write_char('{');
      serialize(ser, value.first);
      ser.write_char(':');
      reflex::visit([&ser](const auto& v) { serialize(ser, v); }, value.second);
      ser.write_char('}');
      return ser.out();
    }

    template <map_c Map>
    friend OutputIt tag_invoke(tag_t<serde::serialize>, serializer<OutputIt>& ser, Map const& value)
    {
      ser.write_char('{');

      bool first = true;

      for(const auto& [key, val] : value)
      {
        if(not first)
        {
          ser.write_char(',');
        }
        else
        {
          first = false;
        }
        serialize(ser, key);
        ser.write_char(':');
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
      ser.write_char('}');
      return ser.out();
    }

    template <visitable_c T>
    friend OutputIt tag_invoke(tag_t<serde::serialize>, serializer<OutputIt>& ser, T const& value)
    {
      return visit([&ser](const auto& v) { return serialize(ser, v); }, value);
    }
  };

  template <typename... TArgs>
  serializer(std::basic_string<TArgs...> & out)
      -> serializer<std::back_insert_iterator<std::basic_string<TArgs...>>>;
  serializer(std::ofstream & out) -> serializer<std::ostreambuf_iterator<char>>;
  serializer(std::ostringstream & out) -> serializer<std::ostreambuf_iterator<char>>;

  template <typename OutputIt, aggregate_c Agg, typename TagT = std::false_type>
    requires(not(str_c<Agg> or seq_c<Agg>)) // std::array<char> may be considered as an aggregate
  OutputIt tag_invoke(
      tag_default_t<serde::serialize>, serializer<OutputIt> & ser, Agg const& value, TagT = {})
  {
    ser.write_char('{');

    constexpr bool tag = TagT::value;

    bool first = not tag;

    if constexpr(tag)
    {
      ser.write_raw("\"__type\":");
      // Hashing at compile time would change the emitted id, which is a wire format
      // break.
      static auto expected_id = std::hash<std::string_view>{}(identifier_of(dealias(decay(^^Agg))));
      serialize(ser, expected_id);
    }

    static constexpr auto type = decay(type_of(^^value));
    template for(constexpr auto member : define_static_array(
                     nonstatic_data_members_of(type, std::meta::access_context::current())))
    {
      if(not first)
      {
        ser.write_char(',');
      }
      else
      {
        first = false;
      }
      static constexpr std::string_view key_token = detail::quoted_key<member>();
      ser.write_raw(key_token);
      auto const& member_value = value.[:member:];
      reflex::visit([&ser](const auto& v) { serialize(ser, v); }, member_value);
    }
    ser.write_char('}');
    return ser.out();
  }

  template <std::input_iterator InputIt>
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
      while(not at_end() and reflex::is_space(*cursor_.begin()))
      {
        cursor_.advance(1);
      }
    }

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

    friend auto tag_invoke(tag_t<serde::deserialize>, deserializer<InputIt>& de)
    {
      return deserialize(de, std::type_identity<json::value>{});
    }

    friend auto tag_invoke(
        tag_t<serde::deserialize>,
        deserializer<InputIt>& de,
        std::type_identity<json::null_t>)
    {
      de.expect("null");
      return null;
    }

    template <typename T>
    friend std::optional<T> tag_invoke(
        tag_t<serde::deserialize>,
        deserializer<InputIt>& de,
        std::type_identity<std::optional<T>>)
    {
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
      requires serde::detail::borrowed_string_sink_c<Str> and deserializer<InputIt>::bulk_scan
    friend Str
        tag_invoke(tag_t<serde::deserialize>, deserializer<InputIt>& de, std::type_identity<Str>)
    {
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
              and (not(serde::detail::borrowed_string_sink_c<Str>
                       and deserializer<InputIt>::bulk_scan))
    friend Str
        tag_invoke(tag_t<serde::deserialize>, deserializer<InputIt>& de, std::type_identity<Str>)
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
    friend auto
        tag_invoke(tag_t<serde::deserialize>, deserializer<InputIt>& de, std::type_identity<Str>)
    {
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

      if constexpr(deserializer<InputIt>::bulk_scan)
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
    friend auto
        tag_invoke(tag_t<serde::deserialize>, deserializer<InputIt>& de, std::type_identity<Num>)
    {
      Num value;
      if constexpr(deserializer<InputIt>::bulk_scan)
      {
        const auto first = std::to_address(de.begin());
        const auto last  = std::to_address(de.end());
        auto [ptr, ec]   = std::from_chars(first, last, value);
        // A number is followed by more document, so the end of the input is not
        // the bound: what must follow is a structural character or whitespace.
        // Anything else, `1.2.3` being the case that used to slip through, is
        // trailing garbage and is rejected here rather than left for the next
        // token to fail on with an unrelated message.
        if(ec != std::errc{} or (ptr != last and not detail::ends_number(*ptr)))
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
        if(ec != std::errc{} or ptr != token.data() + token.size()
           or (not de.at_end() and not detail::ends_number(de.peek())))
        {
          throw std::runtime_error("Failed to parse number");
        }
      }
      return value;
    }

    friend auto tag_invoke(
        tag_t<serde::deserialize>,
        deserializer<InputIt>& de,
        std::type_identity<boolean>)
    {
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
    friend auto
        tag_invoke(tag_t<serde::deserialize>, deserializer<InputIt>& de, std::type_identity<T>)
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
    friend auto
        tag_invoke(tag_t<serde::deserialize>, deserializer<InputIt>& de, std::type_identity<Seq>)
    {
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

      auto push = [&value] {
        if constexpr(requires { value.push_back(elem_type{}); })
        {
          return [&value](elem_type&& elem) { value.push_back(std::forward<elem_type>(elem)); };
        }
        else
        {
          return [it = std::begin(value), end = std::end(value)](elem_type&& elem) mutable {
            if(it == end)
            {
              throw std::out_of_range("Array has more elements than target type can hold");
            }
            *it++ = std::forward<elem_type>(elem);
          };
        }
      }();

      while(true)
      {
        push(de.template load<elem_type>());

        de.ltrim();
        const char sep = de.advance();
        if(sep == ',')
        {
          de.ltrim();
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
    friend auto tag_invoke(
        tag_t<serde::deserialize>,
        deserializer<InputIt>& de,
        std::type_identity<var_type>) -> var_type
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

  template <typename InputIt, object_visitable_c Map>
    requires(
        not(meta::is_template_instance_of(^^Map, ^^poly::var)
            // std::array<char> may be considered as a visitable object
            or str_c<Map>
            or seq_c<Map>))
  auto tag_invoke(
      tag_default_t<serde::deserialize>, deserializer<InputIt> & de, std::type_identity<Map>)
  {
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

      serde::object_visit_flat(key, value, [&]<typename V>(V& v) {
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
                if(type_key->second
                   == std::hash<std::string_view>{}(identifier_of(dealias(decay(a)))))
                {
                  using Agg = [:a:];
                  Agg agg_value{};
                  template for(constexpr auto member :
                               define_static_array(nonstatic_data_members_of(
                                   a, std::meta::access_context::current())))
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
      });

      de.ltrim();
      const char sep = de.advance();
      if(sep == ',')
      {
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
