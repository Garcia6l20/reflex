#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <charconv>
#include <cstring>

#include <reflex/concepts.hpp>
#include <reflex/enum.hpp>
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

  // Marks a member as an XML attribute rather than a child element:
  //   struct Price { [[= xml::attribute]] std::string currency; double amount; };
  //   -> <Price currency="USD"><amount>42.5</amount></Price>
  constexpr struct attribute_t
  {
  } attribute;

  // Marks the member holding the element's text content (attributes + text, no
  // child elements):
  //   struct Price { [[= xml::attribute]] std::string currency;
  //                  [[= xml::text]] double amount; };  -> <Price currency="USD">42.5</Price>
  constexpr struct text_t
  {
  } text;

  // Marks a member capturing the element's inner XML verbatim (escape hatch for
  // mixed content). Read/written unparsed; caller owns well-formedness.
  constexpr struct raw_content_t
  {
  } raw_content;

  // Marks a str_c member to be written inside a CDATA section instead of being
  // entity-escaped:  [[= xml::cdata]] std::string script;
  //   -> <script><![CDATA[...]]></script>. CDATA is read transparently, so this
  // annotation only affects serialization.
  constexpr struct cdata_t
  {
  } cdata;

  consteval bool is_attribute(meta::info member)
  {
    return meta::has_annotation(member, ^^attribute_t);
  }

  consteval bool is_cdata(meta::info member)
  {
    return meta::has_annotation(member, ^^cdata_t);
  }

  // Prefix-based XML namespace. Annotate an aggregate type to prefix its element
  // and its child-element tags and emit an xmlns declaration on its open tag; a
  // member-level annotation overrides the prefix for that element/attribute:
  //   struct[[= xml::ns{"x", "http://e"}]] Env { ... };
  //   -> <x:Env xmlns:x="http://e"><x:child>...</x:child></x:Env>
  struct ns
  {
    constant_string prefix;
    constant_string uri;
  };

  // The namespace prefix declared on a type or member, or "" if none.
  consteval std::string_view ns_prefix_of(meta::info r)
  {
    if(meta::has_annotation(r, ^^ns))
    {
      return std::string_view{meta::annotation_value_of_with<ns>(r).prefix};
    }
    return {};
  }

  consteval std::string_view ns_uri_of(meta::info r)
  {
    if(meta::has_annotation(r, ^^ns))
    {
      return std::string_view{meta::annotation_value_of_with<ns>(r).uri};
    }
    return {};
  }

  consteval bool is_text(meta::info member)
  {
    return meta::has_annotation(member, ^^text_t);
  }

  consteval bool is_raw_content(meta::info member)
  {
    return meta::has_annotation(member, ^^raw_content_t);
  }

  // A type using xml::text or xml::raw_content carries its content in one member
  // instead of child elements.
  template <typename T> consteval bool has_content_member()
  {
    bool found = false;
    template for(constexpr auto member : define_static_array(
                     nonstatic_data_members_of(^^T, std::meta::access_context::current())))
    {
      if(is_text(member) or is_raw_content(member))
      {
        found = true;
      }
    }
    return found;
  }

  namespace detail
  {
  using serde::detail::field_value;

  // An attribute value is a scalar text, or an optional of one. Sequences and
  // aggregates have no attribute representation.
  template <typename T>
  concept xml_attr_c = xml_text_c<T> or (optional_c<T> and xml_text_c<typename field_value<T>::type>);

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
    bool ok       = true;
    int  texts    = 0;
    int  raws     = 0;
    int  children = 0;
    template for(constexpr auto member : define_static_array(
                     nonstatic_data_members_of(^^T, std::meta::access_context::current())))
    {
      using F = std::remove_cvref_t<typename[:type_of(member):]>;
      // cdata is a string-only serialization hint, orthogonal to placement
      if constexpr(is_cdata(member) and not str_c<F>)
      {
        ok = false;
      }
      if constexpr(is_attribute(member))
      {
        // an attribute member must carry a scalar value, not a subtree
        if constexpr(not xml_attr_c<F>)
        {
          ok = false;
        }
      }
      else if constexpr(is_text(member))
      {
        ++texts;
        if constexpr(not xml_attr_c<F>) // text is a scalar or optional scalar
        {
          ok = false;
        }
      }
      else if constexpr(is_raw_content(member))
      {
        ++raws;
        if constexpr(not str_c<F>)
        {
          ok = false;
        }
      }
      else if constexpr(not xml_field_c<F>)
      {
        ++children;
        ok = false;
      }
      else
      {
        ++children;
      }
    }
    // at most one content member, and content is exclusive with child elements
    if(texts > 1 or raws > 1 or (texts > 0 and raws > 0))
    {
      ok = false;
    }
    if((texts + raws) > 0 and children > 0)
    {
      ok = false;
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

  // The local part of a qualified name: "prefix:local" -> "local".
  constexpr std::string_view local_name(std::string_view qname)
  {
    const auto colon = qname.find(':');
    return colon == std::string_view::npos ? qname : qname.substr(colon + 1);
  }

  // "prefix:local", or just "local" when prefix is empty. consteval: every
  // caller has a constant prefix and a constant local part, so the result is
  // promoted to static storage instead of being rebuilt per element.
  consteval std::string_view qualified(std::string_view prefix, std::string_view local)
  {
    if(prefix.empty())
    {
      return {std::define_static_string(local), local.size()};
    }
    std::string s;
    s.reserve(prefix.size() + 1 + local.size());
    s += prefix;
    s += ':';
    s += local;
    return {std::define_static_string(s), s.size()};
  }

  // An xmlns / xmlns:* declaration is namespace machinery, never a field.
  constexpr bool is_xmlns(std::string_view name)
  {
    return name == "xmlns" or name.starts_with("xmlns:");
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

  // Offset of the first byte of `set` at or after `pos`, npos if there is none.
  // `s` must be a bounded value, a text node or an attribute value, never the
  // remaining input: scanning to EOF for a byte that is not there turns a linear
  // parse quadratic.
  inline std::size_t find_any(std::string_view s, std::size_t pos, std::string_view set)
  {
    std::size_t best = std::string_view::npos;
    for(char c : set)
    {
      const std::size_t n = s.find(c, pos);
      if(n < best)
      {
        best = n;
      }
    }
    return best;
  }

  template <typename Ser> void write_text_escaped(Ser& ser, std::string_view text)
  {
    std::size_t pos = 0;
    while(pos < text.size())
    {
      const std::size_t n = find_any(text, pos, "&<>");
      if(n == std::string_view::npos)
      {
        ser.write_raw(text.substr(pos));
        return;
      }
      if(n > pos)
      {
        ser.write_raw(text.substr(pos, n - pos));
      }
      switch(text[n])
      {
        case '&':
          ser.write_raw("&amp;");
          break;
        case '<':
          ser.write_raw("&lt;");
          break;
        default:
          ser.write_raw("&gt;");
      }
      pos = n + 1;
    }
  }

  // Attribute-value escaping: '&', '<', and the delimiting '"'.
  template <typename Ser> void write_attr_escaped(Ser& ser, std::string_view text)
  {
    std::size_t pos = 0;
    while(pos < text.size())
    {
      const std::size_t n = find_any(text, pos, "&<\"");
      if(n == std::string_view::npos)
      {
        ser.write_raw(text.substr(pos));
        return;
      }
      if(n > pos)
      {
        ser.write_raw(text.substr(pos, n - pos));
      }
      switch(text[n])
      {
        case '&':
          ser.write_raw("&amp;");
          break;
        case '<':
          ser.write_raw("&lt;");
          break;
        default:
          ser.write_raw("&quot;");
      }
      pos = n + 1;
    }
  }

  template <bool Attr, typename Ser> void write_escaped(Ser& ser, std::string_view text)
  {
    if constexpr(Attr)
    {
      write_attr_escaped(ser, text);
    }
    else
    {
      write_text_escaped(ser, text);
    }
  }

  // 64 bytes covers the shortest round-trip form of every type to_chars accepts
  // here, so the result is never truncated.
  template <typename Ser, typename N> void write_digits(Ser& ser, N value)
  {
    char       buf[64];
    const auto r = std::to_chars(buf, buf + sizeof(buf), value);
    ser.write_raw(std::string_view{buf, static_cast<std::size_t>(r.ptr - buf)});
  }

  // A scalar straight into the sink. field_text survives only for the
  // Format-derived case, which needs std::format to render.
  template <bool Attr = false, typename Ser, typename F>
  void write_scalar(Ser& ser, F const& value)
  {
    if constexpr(array_of_c<F>) // std::array<char, N>, trimmed at the first NUL
    {
      write_escaped<Attr>(ser, {value.data(), ::strnlen(value.data(), value.size())});
    }
    else if constexpr(str_c<F>)
    {
      write_escaped<Attr>(ser, std::string_view{value});
    }
    else if constexpr(std::same_as<F, bool>)
    {
      ser.write_raw(value ? "true" : "false");
    }
    else if constexpr(std::same_as<F, char>)
    {
      write_escaped<Attr>(ser, std::string_view{&value, 1});
    }
    else if constexpr(number_c<F>)
    {
      // Two-argument to_chars is shortest-round-trip, which is what "{}" is
      // specified to produce, so the rendered bytes are the same. Digits never need
      // escaping.
      write_digits(ser, value);
    }
    else if constexpr(derives_c<F, derive_t<Format>>)
    {
      // ahead of enum_c on purpose: a Format-derived enum renders through its
      // own formatter, not as its underlying value
      write_escaped<Attr>(ser, field_text(value));
    }
    else if constexpr(enum_c<F>)
    {
      write_digits(ser, std::to_underlying(value));
    }
    else
    {
      static_assert(false, std::string(display_string_of(^^F)) + " has no XML text mapping");
    }
  }

  template <typename Ser> void write_tag(Ser& ser, std::string_view name, bool closing)
  {
    if(closing)
    {
      ser.write_raw("</");
    }
    else
    {
      ser.write_char('<');
    }
    ser.write_raw(name);
    ser.write_char('>');
  }

  // Emit CDATA content, splitting any "]]>" across two sections (the only
  // sequence a CDATA section cannot contain): "]]" + reopen + ">".
  template <typename Ser> void write_cdata_content(Ser& ser, std::string_view text)
  {
    for(std::size_t i = 0; i < text.size(); ++i)
    {
      if(text[i] == '>' and i >= 2 and text[i - 1] == ']' and text[i - 2] == ']')
      {
        ser.write_raw("]]><![CDATA[>");
      }
      else
      {
        ser.write_char(text[i]);
      }
    }
  }

  // Element text content: a CDATA section when AsCdata, else entity-escaped.
  template <bool AsCdata, typename Ser, typename F> void write_text_body(Ser& ser, F const& value)
  {
    if constexpr(AsCdata)
    {
      ser.write_raw("<![CDATA[");
      write_cdata_content(ser, std::string_view{value});
      ser.write_raw("]]>");
    }
    else
    {
      write_scalar(ser, value);
    }
  }

  // <name><![CDATA[...]]></name>
  template <typename Ser>
  void write_cdata_element(Ser& ser, std::string_view name, std::string_view text)
  {
    write_tag(ser, name, false);
    ser.write_raw("<![CDATA[");
    write_cdata_content(ser, text);
    ser.write_raw("]]>");
    write_tag(ser, name, true);
  }

  // A member -> zero (empty optional) or one ` name="value"` pair in the open tag.
  template <typename Ser, typename F>
  void write_attribute(Ser& ser, std::string_view name, F const& value)
  {
    if constexpr(optional_c<F>)
    {
      if(value.has_value())
      {
        write_attribute(ser, name, *value);
      }
      // an empty optional attribute is omitted
    }
    else
    {
      ser.write_char(' ');
      ser.write_raw(name);
      ser.write_char('=');
      ser.write_char('"');
      write_scalar<true>(ser, value);
      ser.write_char('"');
    }
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
      const auto t = trim(text);
      // A Format-derived enum is written through its own formatter, by name, so
      // it has to be read back by name. This mirrors the two enum formatters in
      // reflex/enum.hpp: a flags enum renders as "a|b", a plain one as a single
      // enumerator identifier. An enum without the Format derive is written as
      // its underlying integer and is not affected. Integers stay acceptable
      // either way, so this only widens what parses.
      if constexpr(derives_c<F, derive_t<Format>>)
      {
        if constexpr(enum_flags_c<F>)
        {
          using U      = std::underlying_type_t<F>;
          U          bits = 0;
          bool       all  = not t.empty();
          for(std::string_view s = t; not s.empty() and all;)
          {
            const std::string_view token = s.substr(0, s.find('|'));
            s.remove_prefix(std::min(s.size(), token.size() + 1));
            if(const auto v = reflex::to_enum_value<F>(trim(token)))
            {
              bits |= std::to_underlying(*v);
            }
            else
            {
              all = false;
            }
          }
          if(all) return static_cast<F>(bits);
        }
        else if(const auto v = reflex::to_enum_value<F>(t))
        {
          return *v;
        }
      }
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
    detail::write_tag(ser, name, false);
    detail::write_scalar(ser, value);
    detail::write_tag(ser, name, true);
    return ser.out();
  }

  // Aggregate: <name> + one child element per member + </name>. Members recurse
  // through serialize, so nested user overrides win at any depth.
  template <typename OutputIt, xml_element_c Agg>
  OutputIt tag_invoke(
      tag_default_t<serde::serialize>, serializer<OutputIt> & ser, Agg const& value)
  {
    constexpr std::string_view type_prefix = ns_prefix_of(^^Agg);
    constexpr std::string_view type_uri    = ns_uri_of(^^Agg);
    // at the document root a namespaced type prefixes its own type name
    constexpr std::string_view type_default =
        detail::qualified(type_prefix, identifier_of(dealias(decay(^^Agg))));
    const std::string_view name = ser.element_name(type_default);

    // open tag, with an xmlns declaration and attribute members folded in
    ser.write_char('<');
    ser.write_raw(name);
    if constexpr(not type_prefix.empty())
    {
      ser.write_raw(" xmlns:");
      ser.write_raw(type_prefix);
      ser.write_char('=');
      ser.write_char('"');
      detail::write_attr_escaped(ser, type_uri);
      ser.write_char('"');
    }
    template for(constexpr auto member : define_static_array(
                     nonstatic_data_members_of(^^Agg, std::meta::access_context::current())))
    {
      if constexpr(is_attribute(member))
      {
        // attributes take a prefix only from a member-level ns annotation
        constexpr std::string_view apfx = ns_prefix_of(member);
        constexpr std::string_view aqn  = detail::qualified(apfx, serialized_name(member));
        detail::write_attribute(ser, aqn, value.[:member:]);
      }
    }

    if constexpr(has_content_member<Agg>())
    {
      // element body is a single text/raw member, not child elements
      template for(constexpr auto member : define_static_array(
                       nonstatic_data_members_of(^^Agg, std::meta::access_context::current())))
      {
        if constexpr(is_text(member))
        {
          using F                = std::remove_cvref_t<decltype(value.[:member:])>;
          constexpr bool as_cdata = is_cdata(member);
          if constexpr(optional_c<F>)
          {
            if(value.[:member:].has_value())
            {
              ser.write_char('>');
              detail::write_text_body<as_cdata>(ser, value.[:member:].value());
              detail::write_tag(ser, name, true);
            }
            else
            {
              // absent text -> self-closing <name .../>
              ser.write_raw("/>");
            }
          }
          else
          {
            ser.write_char('>');
            detail::write_text_body<as_cdata>(ser, value.[:member:]);
            detail::write_tag(ser, name, true);
          }
        }
        else if constexpr(is_raw_content(member))
        {
          ser.write_char('>');
          ser.write_raw(std::string_view{value.[:member:]}); // inner XML verbatim
          detail::write_tag(ser, name, true);
        }
      }
    }
    else
    {
      ser.write_char('>');
      // non-attribute members as child elements
      template for(constexpr auto member : define_static_array(
                       nonstatic_data_members_of(^^Agg, std::meta::access_context::current())))
      {
        if constexpr(is_attribute(member))
        {
          // already folded into the open tag
        }
        else
        {
          // a child element takes a member-level prefix, else the type prefix
          constexpr std::string_view mpfx = ns_prefix_of(member);
          constexpr std::string_view cpfx = mpfx.empty() ? type_prefix : mpfx;
          constexpr std::string_view cqn  = detail::qualified(cpfx, serialized_name(member));
          if constexpr(is_cdata(member))
          {
            detail::write_cdata_element(ser, cqn, std::string_view{value.[:member:]});
          }
          else
          {
            detail::write_field(ser, cqn, value.[:member:]);
          }
        }
      }
      detail::write_tag(ser, name, true);
    }
    return ser.out();
  }

  template <std::input_iterator InputIt>
  class deserializer : public serde::detail::subrange_deserializer<InputIt>
  {
    using base = serde::detail::subrange_deserializer<InputIt>;
    using base::cursor_;

  public:
    using attr_list = std::vector<std::pair<std::string, std::string>>;

  private:
    // An element open tag already consumed by read_children, stashed so the
    // matched member's deserialize overload can re-read it via read_open_tag.
    struct open_tag_t
    {
      std::string name;
      bool        self_closing;
      attr_list   attributes;
    };
    std::optional<open_tag_t> pending_tag_{};
    // Attributes of the element last returned by read_open_tag, consumed by the
    // aggregate deserialize overload before it reads child elements.
    attr_list current_attributes_{};
    // read_text consumes the '<' of a following tag when it must peek past it;
    // the next tag reader honors this instead of expecting to consume '<' again.
    bool lt_consumed_{false};

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

    // Scanning a whole run at a time needs the input as bytes in memory. A
    // stream cursor (istreambuf_iterator) has none, so it keeps the original
    // character-at-a-time path.
    static constexpr bool bulk_scan =
        std::contiguous_iterator<InputIt> and std::same_as<std::iter_value_t<InputIt>, char>;

    // Internal tag-name carrier: borrowed from the input when it is in memory,
    // owned otherwise. Never escapes the deserializer.
    using name_t = std::conditional_t<bulk_scan, std::string_view, std::string>;

    // The unconsumed input. Every scan below is bounded by what it is about to
    // consume: a run that is scanned is always then advanced over, so the parse
    // stays linear. Searching the whole remaining input for a byte that is not
    // there would rescan to EOF on every call and make it quadratic.
    std::string_view rest() const
      requires bulk_scan
    {
      return {std::to_address(cursor_.begin()),
              static_cast<std::size_t>(cursor_.end() - cursor_.begin())};
    }

    void skip(std::size_t n)
    {
      cursor_.advance(static_cast<std::iter_difference_t<InputIt>>(n));
    }

    // The one place the text path and the attribute path decide whether a raw
    // run can be taken verbatim, so the two cannot drift apart.
    static bool has_entity(std::string_view span)
    {
      return span.find('&') != std::string_view::npos;
    }

    void skip_space()
    {
      while(not at_end() and reflex::is_space(peek())) advance();
    }

    // Consume input until the terminator sequence has been fully matched.
    // KMP so overlapping prefixes (e.g. "--->" against "-->") match correctly.
    // A single-character terminator needs no table and can go through memchr.
    void skip_until(std::string_view term)
    {
      if constexpr(bulk_scan)
      {
        if(term.size() == 1)
        {
          const std::string_view sv = rest();
          const std::size_t      n  = sv.find(term[0]);
          skip(n == std::string_view::npos ? sv.size() : n + 1);
          return;
        }
      }
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

    name_t read_name()
    {
      if constexpr(bulk_scan)
      {
        // find_first_of rather than a min over find(): a name ends within a few bytes,
        // and each separate memchr pass would run to the end of the tail before the
        // min could discard it.
        const std::string_view sv = rest();
        const std::size_t      n  = sv.find_first_of(" \t\n\v\f\r>/?");
        const std::string_view name = (n == std::string_view::npos) ? sv : sv.substr(0, n);
        skip(name.size());
        return name;
      }
      else
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
    }

    // The tag name has been consumed; parse `name="value"` pairs up to '>'.
    // Returns {self_closing, attributes}. Values accept single or double quotes
    // and have their entity references unescaped.
    std::pair<bool, attr_list> read_attributes()
    {
      attr_list attrs;
      while(true)
      {
        skip_space();
        if(at_end())
        {
          throw std::runtime_error("Unterminated XML tag");
        }
        const char c = peek();
        if(c == '>')
        {
          advance();
          return {false, std::move(attrs)};
        }
        if(c == '/')
        {
          advance();
          skip_until(">");
          return {true, std::move(attrs)};
        }

        std::string aname;
        while(not at_end())
        {
          const char d = peek();
          if(reflex::is_space(d) or d == '=' or d == '>' or d == '/')
          {
            break;
          }
          aname.push_back(d);
          advance();
        }

        skip_space();
        std::string avalue;
        if(not at_end() and peek() == '=')
        {
          advance();
          skip_space();
          if(not at_end() and (peek() == '"' or peek() == '\''))
          {
            const char quote = advance();
            bool       taken = false;
            if constexpr(bulk_scan)
            {
              // the value is bounded by its closing quote; a body with no
              // entity is one span and is taken whole
              const std::string_view sv = rest();
              const std::size_t      q  = sv.find(quote);
              const std::string_view body = (q == std::string_view::npos) ? sv : sv.substr(0, q);
              if(not has_entity(body))
              {
                avalue.assign(body);
                skip(body.size());
                if(q != std::string_view::npos) skip(1); // closing quote
                taken = true;
              }
            }
            if(not taken)
            {
              // entity decode, still bounded by the closing quote
              while(not at_end() and peek() != quote)
              {
                const char e = advance();
                if(e == '&')
                {
                  read_entity(avalue);
                }
                else
                {
                  avalue.push_back(e);
                }
              }
              if(not at_end()) advance(); // closing quote
            }
          }
        }

        if(not aname.empty())
        {
          attrs.emplace_back(std::move(aname), std::move(avalue));
        }
      }
    }

    enum class tag_kind
    {
      open,
      close,
      end // end of input
    };
    struct tag_head
    {
      tag_kind  kind;
      name_t    name;
      bool      self_closing;
      attr_list attributes; // open tags only
    };

    // The one tag reader every loop is built on. Skips text, whitespace, XML
    // declarations, comments, CDATA sections, and DOCTYPE, then classifies the
    // next markup as an open tag, a close tag, or end of input.
    tag_head read_tag_head()
    {
      while(true)
      {
        if(lt_consumed_)
        {
          lt_consumed_ = false; // read_text already consumed this tag's '<'
        }
        else
        {
          if constexpr(bulk_scan)
          {
            const std::string_view sv = rest();
            const std::size_t      n  = sv.find('<');
            skip(n == std::string_view::npos ? sv.size() : n);
          }
          else
          {
            while(not at_end() and peek() != '<') advance();
          }
          if(at_end()) return {tag_kind::end, {}, false, {}};
          advance(); // '<'
        }
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
          skip_until(">"); // the name is never read for a close tag
          return {tag_kind::close, {}, false, {}};
        }
        name_t name                = read_name();
        auto [self_closing, attrs] = read_attributes();
        return {tag_kind::open, std::move(name), self_closing, std::move(attrs)};
      }
    }

    // Reads the next element open tag. If a parent stashed an already-consumed
    // open tag, that is returned first. Returns {name, self_closing}; the open
    // tag's attributes are available via attributes() until the next open tag.
    std::pair<std::string, bool> read_open_tag()
    {
      if(pending_tag_)
      {
        auto tag            = *std::exchange(pending_tag_, std::nullopt);
        current_attributes_ = std::move(tag.attributes);
        return {std::move(tag.name), tag.self_closing};
      }
      tag_head head = read_tag_head();
      if(head.kind != tag_kind::open)
      {
        throw std::runtime_error("Expected an XML element");
      }
      current_attributes_ = std::move(head.attributes);
      return {std::string(head.name), head.self_closing};
    }

    // Attributes of the element whose open tag read_open_tag last returned.
    const attr_list& attributes() const
    {
      return current_attributes_;
    }

    void read_close_tag()
    {
      if(read_tag_head().kind != tag_kind::close)
      {
        throw std::runtime_error("Expected an XML close tag");
      }
    }

    void expect(std::string_view token)
    {
      for(char e : token)
      {
        if(at_end() or advance() != e)
        {
          throw std::runtime_error(std::format("Expected \"{}\"", token));
        }
      }
    }

    // "<![CDATA[" has been consumed; append raw content up to "]]>" (dropped).
    void read_cdata(std::string& out)
    {
      const std::size_t start = out.size();
      while(not at_end())
      {
        out.push_back(advance());
        const std::size_t n = out.size();
        if(n - start >= 3 and out[n - 3] == ']' and out[n - 2] == ']' and out[n - 1] == '>')
        {
          out.resize(n - 3); // drop the "]]>" terminator
          return;
        }
      }
    }

    // Element text content up to the terminating tag, with entity references
    // unescaped and CDATA sections taken verbatim. Plain text and any number of
    // CDATA sections concatenate. Stops at the first non-CDATA tag ('<' already
    // consumed, flagged via lt_consumed_ for the next tag reader).
    std::string read_text()
    {
      std::string text;
      while(not at_end())
      {
        if constexpr(bulk_scan)
        {
          // The run ends at the next '<', located once and then reused as the bound for
          // every '&' search inside it. Relocating it per entity would rescan the tail
          // once per entity and make an entity-dense text node quadratic.
          const std::string_view sv       = rest();
          const std::size_t      lt       = sv.find('<');
          const std::size_t      head_len = (lt == std::string_view::npos) ? sv.size() : lt;
          if(head_len > 0)
          {
            const char* const end = sv.data() + head_len;
            while(true)
            {
              // read_entity stops at '<', so the cursor never passes `end`
              const char* const cur = std::to_address(cursor_.begin());
              if(cur >= end) break;
              const std::string_view head{cur, static_cast<std::size_t>(end - cur)};
              const std::size_t amp = head.find('&');
              if(amp == std::string_view::npos)
              {
                text.append(head);
                skip(head.size());
                break;
              }
              if(amp > 0)
              {
                text.append(head.substr(0, amp));
                skip(amp);
              }
              skip(1); // '&'
              read_entity(text);
            }
            continue; // at least one byte consumed, so this cannot spin
          }
          // head_len == 0: the cursor is on a '<', handled below
        }

        if(peek() != '<')
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
          continue;
        }

        // a '<': CDATA and comments are part of the text run; a real tag ends it
        advance(); // '<'
        if(not at_end() and peek() == '!')
        {
          advance(); // '!'
          if(not at_end() and peek() == '[')
          {
            expect("[CDATA[");
            read_cdata(text);
          }
          else if(not at_end() and peek() == '-')
          {
            advance();
            if(not at_end() and peek() == '-') advance();
            skip_until("-->"); // comments are not content
          }
          else
          {
            skip_until(">"); // other declaration, ignored
          }
          continue;
        }

        // a genuine tag (close tag, in a text context): stop, leave it for the
        // next reader, remembering that its '<' is already gone
        lt_consumed_ = true;
        break;
      }
      return text;
    }

    // Read this element's text content and its close tag, parsed into T. The
    // borrowed run is consumed here, so nothing outlives the input buffer.
    template <typename T> T read_text_field()
    {
      text_run run = read_text_run();
      read_close_tag();
      return parse_run<T>(std::move(run));
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

    // Capture inner XML verbatim up to the matching close tag (which is consumed
    // but not returned). Entities left undecoded, byte-exact for the common case.
    // Note: a comment or CDATA payload containing '>' can mis-track.
    std::string read_raw_content()
    {
      std::string raw;
      int         depth = 1;
      while(not at_end())
      {
        while(not at_end() and peek() != '<') raw.push_back(advance());
        if(at_end()) break;

        std::string tag;
        tag.push_back(advance()); // '<'
        const char kind = at_end() ? char{0} : peek();
        char       last = 0;
        while(not at_end())
        {
          const char c = advance();
          tag.push_back(c);
          if(c == '"' or c == '\'')
          {
            const char quote = c;
            while(not at_end() and peek() != quote) tag.push_back(advance());
            if(not at_end()) tag.push_back(advance());
          }
          else if(c == '>')
          {
            break;
          }
          else if(not reflex::is_space(c))
          {
            last = c;
          }
        }

        if(kind == '/')
        {
          if(--depth == 0) break; // matching close tag, dropped from output
          raw += tag;
        }
        else if(kind == '?' or kind == '!')
        {
          raw += tag; // PI/comment/CDATA/doctype -- no depth change
        }
        else
        {
          raw += tag;
          if(last != '/') ++depth; // not self-closing
        }
      }
      return raw;
    }

    // Read the single text/raw content member of an aggregate, then its close
    // tag. Called instead of read_children for a has_content_member type.
    template <detail::aggregate_element_c Agg> void read_content_member(Agg & value)
    {
      template for(constexpr auto member : define_static_array(
                       nonstatic_data_members_of(^^Agg, std::meta::access_context::current())))
      {
        if constexpr(is_text(member))
        {
          using F                     = std::remove_cvref_t<decltype(value.[:member:])>;
          text_run               run  = read_text_run();
          const std::string_view text = run.view();
          read_close_tag();
          if constexpr(optional_c<F>)
          {
            using U = typename detail::field_value<F>::type;
            if(detail::trim(text).empty())
            {
              value.[:member:] = std::nullopt;
            }
            else
            {
              value.[:member:] = parse_run<U>(std::move(run));
            }
          }
          else
          {
            value.[:member:] = parse_run<F>(std::move(run));
          }
        }
        else if constexpr(is_raw_content(member))
        {
          using F          = std::remove_cvref_t<decltype(value.[:member:])>;
          value.[:member:] = F{read_raw_content()};
        }
      }
    }

    // Read one element's value into T. The open tag (name, self_closing, attrs)
    // has been consumed by read_children; stash it and re-enter deserialize so
    // T's own overload (including any user override) handles the element.
    template <typename T> T read_child(name_t name, bool self_closing, attr_list attrs)
    {
      pending_tag_ = open_tag_t{std::string(name), self_closing, std::move(attrs)};
      return deserialize(*this, std::type_identity<T>{});
    }

    // Optional member: an empty or self-closing element holding text is
    // nullopt. A scalar body is read inline; an aggregate/user body routes back
    // through the CPO, self-closing included -- <Range Min="1" Max="2"/> has no
    // body but still carries a value in its attributes.
    template <typename U>
    std::optional<U> read_optional(name_t name, bool self_closing, attr_list attrs)
    {
      if constexpr(xml_text_c<U>)
      {
        if(self_closing) return std::nullopt;
        text_run               run  = read_text_run();
        const std::string_view text = run.view();
        read_close_tag();
        if(detail::trim(text).empty()) return std::nullopt;
        return parse_run<U>(std::move(run));
      }
      else
      {
        return read_child<U>(std::move(name), self_closing, std::move(attrs));
      }
    }

    // Assign an element's attributes to the aggregate's attribute members,
    // matched by serialized name. Unknown attributes are ignored.
    template <detail::aggregate_element_c Agg> void assign_attributes(Agg & value)
    {
      for(auto const& [aname, avalue] : current_attributes_)
      {
        const std::string_view an = aname;
        if(detail::is_xmlns(an)) continue; // namespace declarations are not fields

        const std::string_view local = detail::local_name(an);
        bool                   any_exact = false;
        template for(constexpr auto member : define_static_array(
                         nonstatic_data_members_of(^^Agg, std::meta::access_context::current())))
        {
          if constexpr(is_attribute(member))
          {
            if(serialized_name(member) == an or identifier_of(member) == an) any_exact = true;
          }
        }

        bool matched = false;
        template for(constexpr auto member : define_static_array(
                         nonstatic_data_members_of(^^Agg, std::meta::access_context::current())))
        {
          if constexpr(is_attribute(member))
          {
            const bool exact = serialized_name(member) == an or identifier_of(member) == an;
            const bool loc   = not any_exact
                           and (serialized_name(member) == local or identifier_of(member) == local);
            if(not matched and (exact or loc))
            {
              using F        = std::remove_cvref_t<decltype(value.[:member:])>;
              value.[:member:] = detail::parse_field<F>(avalue);
              matched        = true;
            }
          }
        }
      }
    }

    // Loop over child elements of an aggregate, dispatching each by name.
    // Consumes the parent's close tag before returning.
    template <detail::aggregate_element_c Agg> void read_children(Agg & value)
    {
      while(true)
      {
        tag_head head = read_tag_head();
        // a close tag ends this element; end of input tolerates truncation
        if(head.kind != tag_kind::open) return;

        const std::string_view name         = head.name;
        const std::string_view local        = detail::local_name(name);
        const bool             self_closing = head.self_closing;
        bool                   matched      = false;

        // Prefer an exact (possibly qualified) match; only fall back to the
        // local name when no member matches the qualified name.
        bool any_exact = false;
        template for(constexpr auto member : define_static_array(
                         nonstatic_data_members_of(^^Agg, std::meta::access_context::current())))
        {
          if constexpr(not is_attribute(member))
          {
            if(serialized_name(member) == name or identifier_of(member) == name) any_exact = true;
          }
        }

        template for(constexpr auto member : define_static_array(
                         nonstatic_data_members_of(^^Agg, std::meta::access_context::current())))
        {
          // attribute members are filled from the open tag, never child elements
          if constexpr(not is_attribute(member))
          {
            const bool exact = serialized_name(member) == name or identifier_of(member) == name;
            const bool loc   = not any_exact
                           and (serialized_name(member) == local or identifier_of(member) == local);
            if(not matched and (exact or loc))
            {
              matched     = true;
              using F     = std::remove_cvref_t<decltype(value.[:member:])>;
              if constexpr(seq_c<F> and not array_of_c<F>)
              {
                using E = typename F::value_type;
                value.[:member:].push_back(
                    read_child<E>(head.name, self_closing, std::move(head.attributes)));
              }
              else if constexpr(optional_c<F>)
              {
                using U = typename detail::field_value<F>::type;
                value.[:member:] =
                    read_optional<U>(head.name, self_closing, std::move(head.attributes));
              }
              else
              {
                value.[:member:] =
                    read_child<F>(head.name, self_closing, std::move(head.attributes));
              }
            }
          }
        }

        if(not matched)
        {
          skip_subtree(self_closing);
        }
      }
    }

  private:
    // A text run that is one contiguous span of the source is handed back as a
    // view into it. A run that had to be decoded (entities) or spliced (CDATA),
    // and anything read from a stream, comes back owned. Strictly internal: the
    // view is valid only while the input buffer lives.
    struct text_run
    {
      std::string_view borrowed;
      std::string      owned;
      bool             is_borrowed = false;

      std::string_view view() const
      {
        return is_borrowed ? borrowed : std::string_view{owned};
      }
    };

    template <typename F> static F parse_run(text_run&& run)
    {
      using U = typename detail::field_value<F>::type;
      if constexpr(std::same_as<U, std::string>)
      {
        if(not run.is_borrowed) return F{std::move(run.owned)};
      }
      return detail::parse_field<F>(run.view());
    }

    text_run read_text_run()
    {
      if constexpr(bulk_scan)
      {
        const std::string_view sv   = rest();
        const std::size_t      lt   = sv.find('<');
        const std::string_view head = (lt == std::string_view::npos) ? sv : sv.substr(0, lt);
        // an entity has to be decoded, and "<!" continues the run with a CDATA
        // section or a comment: both need the owning path
        const bool simple = not has_entity(head)
                        and (lt == std::string_view::npos or lt + 1 >= sv.size()
                             or sv[lt + 1] != '!');
        if(simple)
        {
          skip(head.size());
          if(lt != std::string_view::npos)
          {
            skip(1);             // '<', consumed here
            lt_consumed_ = true; // so the next tag reader does not expect it again
          }
          return {head, {}, true};
        }
      }
      return {{}, read_text(), false};
    }

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

  REFLEX_SERDE_DESERIALIZER_DEDUCTION_GUIDES(deserializer);

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
    return de.template read_text_field<T>();
  }

  template <typename InputIt, xml_element_c Agg>
  Agg tag_invoke(
      tag_default_t<serde::deserialize>, deserializer<InputIt> & de, std::type_identity<Agg>)
  {
    auto [name, self_closing] = de.read_open_tag();
    Agg value{};
    de.assign_attributes(value); // consume open-tag attributes before content
    if(not self_closing)
    {
      if constexpr(has_content_member<Agg>())
      {
        de.read_content_member(value);
      }
      else
      {
        de.read_children(value);
      }
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
