#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <cstring>

#include <reflex/concepts.hpp>
#include <reflex/format.hpp>
#include <reflex/heapless/string.hpp>
#include <reflex/parse.hpp>
#include <reflex/serde.hpp>
#endif

#include <reflex/serde/detail/io.hpp>

REFLEX_EXPORT namespace reflex::serde::xml
{
  // Members recurse through serde::serialize / serde::deserialize, so a
  // user-defined tag_invoke override on a member type wins at any depth. An XML
  // element's name comes from the enclosing member, not the value's type, so the
  // name is passed out of band: the serializer holds a pending-name slot, the
  // deserializer stashes the already-consumed open tag.
  //
  // Override contract: a serialize override emits exactly one element and should
  // name it with ser.element_name("default") to be correct when nested, a
  // deserialize override consumes its whole element (open tag via read_open_tag,
  // body, close tag unless self-closing).

  // A std::array<char, N> is treated as a fixed-capacity string (NUL-trimmed),
  // like csv/json. Other std::array instantiations are not text.
  template <typename T>
  concept char_array_c =
      array_of_c<T> and std::same_as<std::remove_cvref_t<typename std::remove_cvref_t<T>::value_type>, char>;

  // XML text content holds a single scalar value. These map to the element's
  // text node; everything else nests as child elements (see xml_field_c).
  template <typename T>
  concept xml_text_c = number_c<T>
                    or std::same_as<T, bool>
                    or std::same_as<T, char>
                    or enum_c<T>
                    or str_c<T>
                    or char_array_c<T>
                    or derives_c<T, derive_t<Format>>;

  namespace detail
  {
  using serde::detail::field_value;

  template <typename T>
  concept aggregate_element_c =
      aggregate_c<T> and not str_c<T> and not seq_c<T> and not array_of_c<T>;

  // A single element maps to text or a nested aggregate. Sequences of sequences
  // and optionals of sequences have no repeated-element representation.
  template <typename T>
  concept xml_leaf_c = xml_text_c<T> or aggregate_element_c<T>;

  // A member maps to a child element (or repeated elements). It is a leaf, an
  // optional leaf, or a sequence of leaves.
  template <typename T> consteval bool is_field()
  {
    using F = std::remove_cvref_t<T>;
    if constexpr(optional_c<F>)
    {
      return xml_leaf_c<typename field_value<F>::type>;
    }
    else if constexpr(seq_c<F> and not array_of_c<F>)
    {
      return xml_leaf_c<typename F::value_type>;
    }
    else
    {
      return xml_leaf_c<F>;
    }
  }
  } // namespace detail

  template <typename T>
  concept xml_field_c = detail::is_field<std::remove_cvref_t<T>>();

  namespace detail
  {
  template <typename T> consteval bool element_fields_ok()
  {
    bool ok = true;
    template for(constexpr auto member : define_static_array(
                     nonstatic_data_members_of(^^T, std::meta::access_context::current())))
    {
      using F = std::remove_cvref_t<typename[:type_of(member):]>;
      if constexpr(not xml_field_c<F>)
      {
        ok = false;
      }
    }
    return ok;
  }
  } // namespace detail

  // A top-level XML document is an aggregate whose every member is an xml_field_c.
  template <typename T>
  concept xml_element_c = detail::aggregate_element_c<T>
                      and detail::element_fields_ok<std::remove_cvref_t<T>>();

  namespace detail
  {
  constexpr std::string_view trim(std::string_view s)
  {
    while(not s.empty() and reflex::is_space(s.front())) s.remove_prefix(1);
    while(not s.empty() and reflex::is_space(s.back())) s.remove_suffix(1);
    return s;
  }

  template <typename F> std::string field_text(F const& value)
  {
    if constexpr(array_of_c<F>) // std::array<char, N>, trimmed at the first NUL
    {
      return std::string{
          std::string_view{value.data(), ::strnlen(value.data(), value.size())}};
    }
    else if constexpr(str_c<F>)
    {
      return std::string{std::string_view{value}};
    }
    else if constexpr(std::same_as<F, bool>)
    {
      return value ? "true" : "false";
    }
    else if constexpr(std::same_as<F, char>)
    {
      return std::string(1, value);
    }
    else if constexpr(number_c<F>)
    {
      return std::format("{}", value);
    }
    else if constexpr(derives_c<F, derive_t<Format>>)
    {
      return std::format("{}", value);
    }
    else if constexpr(enum_c<F>)
    {
      return std::format("{}", std::to_underlying(value));
    }
    else
    {
      static_assert(false, std::string(display_string_of(^^F)) + " has no XML text mapping");
    }
  }

  template <typename OutputIt> void write_text_escaped(OutputIt& out, std::string_view text)
  {
    for(char c : text)
    {
      switch(c)
      {
        case '&':
          std::ranges::copy(std::string_view{"&amp;"}, out);
          break;
        case '<':
          std::ranges::copy(std::string_view{"&lt;"}, out);
          break;
        case '>':
          std::ranges::copy(std::string_view{"&gt;"}, out);
          break;
        default:
          out++ = c;
      }
    }
  }

  template <typename OutputIt> void write_tag(OutputIt& out, std::string_view name, bool closing)
  {
    out++ = '<';
    if(closing)
    {
      out++ = '/';
    }
    for(char c : name) out++ = c;
    out++ = '>';
  }

  // A member -> zero (empty optional), one, or many (sequence) child elements.
  // The element name reaches the value's serialize overload through the
  // serializer's pending-name slot.
  template <typename Ser, typename F>
  void write_field(Ser& ser, std::string_view name, F const& value)
  {
    if constexpr(optional_c<F>)
    {
      if(value.has_value())
      {
        write_field(ser, name, *value);
      }
      // an empty optional emits no element
    }
    else if constexpr(seq_c<F> and not array_of_c<F>)
    {
      for(auto const& elem : value)
      {
        ser.set_element_name(name);
        serialize(ser, elem);
      }
      // an empty sequence emits no element
    }
    else
    {
      ser.set_element_name(name);
      serialize(ser, value);
    }
  }

  template <typename F> F parse_field(std::string_view text)
  {
    if constexpr(optional_c<F>)
    {
      using U = typename field_value<F>::type;
      return parse_field<U>(text);
    }
    else if constexpr(array_of_c<F>) // std::array<char, N>
    {
      F          arr{};
      const auto n = std::min(text.size(), arr.size());
      std::ranges::copy_n(text.begin(), n, arr.begin());
      return arr;
    }
    else if constexpr(str_c<F>)
    {
      return F{text};
    }
    else if constexpr(std::same_as<F, bool>)
    {
      const auto t = trim(text);
      return t == "true" or t == "1";
    }
    else if constexpr(std::same_as<F, char>)
    {
      return text.empty() ? char{} : text.front();
    }
    else if constexpr(number_c<F>)
    {
      const auto t   = trim(text);
      F          value{};
      auto [ptr, ec] = std::from_chars(t.data(), t.data() + t.size(), value);
      if(ec != std::errc{}) throw std::runtime_error("XML: failed to parse number");
      return value;
    }
    else if constexpr(derives_c<F, derive_t<Parse>>)
    {
      return parse_or_throw<F>(trim(text));
    }
    else if constexpr(enum_c<F>)
    {
      const auto                t = trim(text);
      std::underlying_type_t<F> value{};
      auto [ptr, ec] = std::from_chars(t.data(), t.data() + t.size(), value);
      if(ec != std::errc{}) throw std::runtime_error("XML: failed to parse enum");
      return static_cast<F>(value);
    }
    else
    {
      static_assert(false, std::string(display_string_of(^^F)) + " is not parseable from XML");
    }
  }
  } // namespace detail

  template <typename OutputIt> class serializer : public serde::detail::serializer_base<OutputIt>
  {
    // The element name for the *next* serialize call, set by the enclosing
    // aggregate before it recurses into a member. Consumed at the entry of that
    // call, before any deeper recursion.
    std::string_view pending_name_{};

  public:
    using serde::detail::serializer_base<OutputIt>::serializer_base;

    static constexpr std::string_view format_name = "XML";
    static constexpr std::string_view format_hint =
        "(expected an aggregate whose members are scalars, sequences, optionals, "
        "or nested aggregates)";

    void set_element_name(std::string_view name)
    {
      pending_name_ = name;
    }

    // Fetch-and-clear the pending name, falling back to the type name at the
    // document root (where nothing set a pending name).
    std::string_view element_name(std::string_view type_default)
    {
      const auto name = pending_name_.empty() ? type_default : pending_name_;
      pending_name_   = {};
      return name;
    }
  };

  template <typename... TArgs>
  serializer(std::basic_string<TArgs...> & out)
      -> serializer<std::back_insert_iterator<std::basic_string<TArgs...>>>;
  serializer(std::ofstream & out) -> serializer<std::ostreambuf_iterator<char>>;
  serializer(std::ostringstream & out) -> serializer<std::ostreambuf_iterator<char>>;

  // Leaf: a scalar maps to <name>text</name>. Only reached with an explicit
  // element name except at the document root, where a bare scalar falls back to
  // its type spelling (display_string_of, since scalars have no identifier).
  template <typename OutputIt, xml_text_c T>
  OutputIt tag_invoke(tag_default_t<serde::serialize>, serializer<OutputIt> & ser, T const& value)
  {
    const std::string_view name = ser.element_name(display_string_of(dealias(decay(^^T))));
    auto&                  out  = ser.out();
    detail::write_tag(out, name, false);
    detail::write_text_escaped(out, detail::field_text(value));
    detail::write_tag(out, name, true);
    return out;
  }

  // Aggregate: <name> + one child element per member + </name>. Members recurse
  // through serialize, so nested user overrides win at any depth.
  template <typename OutputIt, xml_element_c Agg>
  OutputIt tag_invoke(
      tag_default_t<serde::serialize>, serializer<OutputIt> & ser, Agg const& value)
  {
    const std::string_view name = ser.element_name(identifier_of(dealias(decay(^^Agg))));
    auto&                  out  = ser.out();
    detail::write_tag(out, name, false);
    template for(constexpr auto member : define_static_array(
                     nonstatic_data_members_of(^^Agg, std::meta::access_context::current())))
    {
      constexpr std::string_view member_name = serialized_name(member);
      detail::write_field(ser, member_name, value.[:member:]);
    }
    detail::write_tag(out, name, true);
    return out;
  }

  template <std::input_iterator InputIt>
  class deserializer : public serde::detail::subrange_deserializer<InputIt>
  {
    using base = serde::detail::subrange_deserializer<InputIt>;
    using base::cursor_;

    // An element open tag already consumed by read_children, stashed so the
    // matched member's deserialize overload can re-read it via read_open_tag.
    struct open_tag_t
    {
      std::string name;
      bool        self_closing;
    };
    std::optional<open_tag_t> pending_tag_{};

  public:
    using base::at_end;
    using base::base;

    char peek() const
    {
      if(at_end())
      {
        throw std::runtime_error("Unexpected end of XML input");
      }
      return *cursor_.begin();
    }

    char advance()
    {
      const char c = peek();
      cursor_.advance(1);
      return c;
    }

    // Consume input until the terminator sequence has been fully matched.
    // KMP so overlapping prefixes (e.g. "--->" against "-->") match correctly.
    void skip_until(std::string_view term)
    {
      std::array<std::size_t, 8> fail{};
      for(std::size_t i = 1, k = 0; i < term.size(); ++i)
      {
        while(k > 0 and term[i] != term[k]) k = fail[k - 1];
        if(term[i] == term[k]) ++k;
        fail[i] = k;
      }
      std::size_t k = 0;
      while(not at_end())
      {
        const char c = advance();
        while(k > 0 and c != term[k]) k = fail[k - 1];
        if(c == term[k]) ++k;
        if(k == term.size()) return;
      }
    }

    // The leading "<!" has been consumed; skip a comment, CDATA section, or
    // DOCTYPE. CDATA ends at "]]>" so a '>' inside its payload is not mistaken
    // for the terminator.
    void skip_bang()
    {
      if(not at_end() and peek() == '-')
      {
        advance();
        if(not at_end() and peek() == '-') advance();
        skip_until("-->");
      }
      else if(not at_end() and peek() == '[')
      {
        skip_until("]]>");
      }
      else
      {
        skip_until(">");
      }
    }

    std::string read_name()
    {
      std::string name;
      while(not at_end())
      {
        const char c = peek();
        if(reflex::is_space(c) or c == '>' or c == '/' or c == '?')
        {
          break;
        }
        name.push_back(c);
        advance();
      }
      return name;
    }

    // The tag name has been consumed; skip attributes up to '>'. Returns true
    // for a self-closing tag ("<name .../>").
    bool read_attributes()
    {
      bool prev_slash = false;
      while(not at_end())
      {
        const char c = advance();
        if(c == '"' or c == '\'')
        {
          const char quote = c;
          while(not at_end() and peek() != quote) advance();
          if(not at_end()) advance();
          prev_slash = false;
        }
        else if(c == '>')
        {
          return prev_slash;
        }
        else if(reflex::is_space(c))
        {
          // whitespace does not reset the self-closing marker
        }
        else
        {
          prev_slash = (c == '/');
        }
      }
      throw std::runtime_error("Unterminated XML tag");
    }

    enum class tag_kind
    {
      open,
      close,
      end // end of input
    };
    struct tag_head
    {
      tag_kind    kind;
      std::string name;
      bool        self_closing;
    };

    // The one tag reader every loop is built on. Skips text, whitespace, XML
    // declarations, comments, CDATA sections, and DOCTYPE, then classifies the
    // next markup as an open tag, a close tag, or end of input.
    tag_head read_tag_head()
    {
      while(true)
      {
        while(not at_end() and peek() != '<') advance();
        if(at_end()) return {tag_kind::end, {}, false};
        advance(); // '<'
        const char d = peek();
        if(d == '?')
        {
          advance();
          skip_until("?>");
          continue;
        }
        if(d == '!')
        {
          advance();
          skip_bang();
          continue;
        }
        if(d == '/')
        {
          advance();
          std::string name = read_name();
          skip_until(">");
          return {tag_kind::close, std::move(name), false};
        }
        std::string name = read_name();
        return {tag_kind::open, std::move(name), read_attributes()};
      }
    }

    // Reads the next element open tag. If a parent stashed an already-consumed
    // open tag, that is returned first. Returns {name, self_closing}.
    std::pair<std::string, bool> read_open_tag()
    {
      if(pending_tag_)
      {
        auto tag = *std::exchange(pending_tag_, std::nullopt);
        return {std::move(tag.name), tag.self_closing};
      }
      tag_head head = read_tag_head();
      if(head.kind != tag_kind::open)
      {
        throw std::runtime_error("Expected an XML element");
      }
      return {std::move(head.name), head.self_closing};
    }

    void read_close_tag()
    {
      if(read_tag_head().kind != tag_kind::close)
      {
        throw std::runtime_error("Expected an XML close tag");
      }
    }

    // Text content up to the next '<', with entity references unescaped.
    std::string read_text()
    {
      std::string text;
      while(not at_end() and peek() != '<')
      {
        const char c = advance();
        if(c == '&')
        {
          read_entity(text);
        }
        else
        {
          text.push_back(c);
        }
      }
      return text;
    }

    // An unknown element: skip its whole subtree. self_closing tags have none.
    void skip_subtree(bool self_closing)
    {
      if(self_closing) return;
      int depth = 1;
      while(depth > 0)
      {
        const tag_head head = read_tag_head();
        if(head.kind == tag_kind::end) return;
        if(head.kind == tag_kind::open)
        {
          if(not head.self_closing) ++depth;
        }
        else
        {
          --depth;
        }
      }
    }

    // Read one element's value into T. The open tag (name, self_closing) has
    // been consumed by read_children; stash it and re-enter deserialize so
    // T's own overload (including any user override) handles the element.
    template <typename T> T read_child(std::string name, bool self_closing)
    {
      pending_tag_ = open_tag_t{std::move(name), self_closing};
      return deserialize(*this, std::type_identity<T>{});
    }

    // Optional member: an empty or self-closing element is nullopt. A scalar
    // body is read inline; an aggregate/user body routes back through the CPO.
    template <typename U>
    std::optional<U> read_optional(std::string name, bool self_closing)
    {
      if(self_closing) return std::nullopt;
      if constexpr(xml_text_c<U>)
      {
        std::string text = read_text();
        read_close_tag();
        if(detail::trim(text).empty()) return std::nullopt;
        return detail::parse_field<U>(text);
      }
      else
      {
        return read_child<U>(std::move(name), false);
      }
    }

    // Loop over child elements of an aggregate, dispatching each by name.
    // Consumes the parent's close tag before returning.
    template <detail::aggregate_element_c Agg> void read_children(Agg & value)
    {
      while(true)
      {
        const tag_head head = read_tag_head();
        // a close tag ends this element; end of input tolerates truncation
        if(head.kind != tag_kind::open) return;

        const std::string_view name         = head.name;
        const bool             self_closing = head.self_closing;
        bool                   matched      = false;

        template for(constexpr auto member : define_static_array(
                         nonstatic_data_members_of(^^Agg, std::meta::access_context::current())))
        {
          if(not matched
             and (serialized_name(member) == name or identifier_of(member) == name))
          {
            matched     = true;
            using F     = std::remove_cvref_t<decltype(value.[:member:])>;
            if constexpr(seq_c<F> and not array_of_c<F>)
            {
              using E = typename F::value_type;
              value.[:member:].push_back(read_child<E>(head.name, self_closing));
            }
            else if constexpr(optional_c<F>)
            {
              using U = typename detail::field_value<F>::type;
              value.[:member:] = read_optional<U>(head.name, self_closing);
            }
            else
            {
              value.[:member:] = read_child<F>(head.name, self_closing);
            }
          }
        }

        if(not matched)
        {
          skip_subtree(self_closing);
        }
      }
    }

    template <typename T> T load()
    {
      return deserialize(*this, std::type_identity<T>{});
    }

  private:
    // A '&' has already been consumed; decode the entity and append to out.
    // On any malformed or unknown reference the original text is preserved
    // verbatim (never silently dropped, never eats following markup).
    void read_entity(std::string& out)
    {
      std::string name;
      while(not at_end())
      {
        const char c = peek();
        if(c == ';' or c == '<' or c == '&' or reflex::is_space(c) or name.size() >= 16)
        {
          break;
        }
        name.push_back(c);
        advance();
      }
      const bool terminated = not at_end() and peek() == ';';
      if(terminated) advance();

      if(terminated)
      {
        if(name == "amp")
        {
          out.push_back('&');
          return;
        }
        if(name == "lt")
        {
          out.push_back('<');
          return;
        }
        if(name == "gt")
        {
          out.push_back('>');
          return;
        }
        if(name == "quot")
        {
          out.push_back('"');
          return;
        }
        if(name == "apos")
        {
          out.push_back('\'');
          return;
        }
        if(not name.empty() and name.front() == '#')
        {
          unsigned long cp   = 0;
          const bool    hex  = name.size() > 1 and (name[1] == 'x' or name[1] == 'X');
          const auto    body = std::string_view{name}.substr(hex ? 2 : 1);
          auto [ptr, ec] =
              std::from_chars(body.data(), body.data() + body.size(), cp, hex ? 16 : 10);
          if(ec == std::errc{} and ptr == body.data() + body.size() and is_valid_codepoint(cp))
          {
            append_utf8(out, cp);
            return;
          }
        }
      }

      // malformed or unknown: keep the raw text
      out.push_back('&');
      out.append(name);
      if(terminated) out.push_back(';');
    }

    static constexpr bool is_valid_codepoint(unsigned long cp)
    {
      return cp <= 0x10FFFF and not(cp >= 0xD800 and cp <= 0xDFFF);
    }

    static void append_utf8(std::string& out, unsigned long cp)
    {
      if(cp < 0x80)
      {
        out.push_back(static_cast<char>(cp));
      }
      else if(cp < 0x800)
      {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
      }
      else if(cp < 0x10000)
      {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
      }
      else
      {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
      }
    }
  };

  template <typename... TArgs>
  deserializer(std::basic_string<TArgs...> const& in)
      -> deserializer<typename std::basic_string<TArgs...>::const_iterator>;

  template <typename... TArgs>
  deserializer(std::basic_string_view<TArgs...> const& in)
      -> deserializer<typename std::basic_string_view<TArgs...>::const_iterator>;

  template <typename CharT, typename CharTrait = std::char_traits<CharT>>
  deserializer(std::basic_istream<CharT, CharTrait>)
      -> deserializer<std::istreambuf_iterator<CharT>>;

  // Leaf: read <name>text</name> (or a self-closing empty element) into a scalar.
  template <typename InputIt, xml_text_c T>
  T tag_invoke(
      tag_default_t<serde::deserialize>, deserializer<InputIt> & de, std::type_identity<T>)
  {
    auto [name, self_closing] = de.read_open_tag();
    if(self_closing)
    {
      return detail::parse_field<T>("");
    }
    std::string text = de.read_text();
    de.read_close_tag();
    return detail::parse_field<T>(text);
  }

  template <typename InputIt, xml_element_c Agg>
  Agg tag_invoke(
      tag_default_t<serde::deserialize>, deserializer<InputIt> & de, std::type_identity<Agg>)
  {
    auto [name, self_closing] = de.read_open_tag();
    Agg value{};
    if(not self_closing)
    {
      de.read_children(value);
    }
    return value;
  }

} // namespace reflex::serde::xml

REFLEX_EXPORT namespace reflex::serde::ser
{
  constexpr auto xml = ^^reflex::serde::xml::serializer;
}

REFLEX_EXPORT namespace reflex::serde::de
{
  constexpr auto xml = ^^reflex::serde::xml::deserializer;
}
