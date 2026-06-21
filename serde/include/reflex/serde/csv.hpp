#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <cstring>

#include <reflex/format.hpp>
#include <reflex/parse.hpp>
#include <reflex/serde.hpp>
#endif

#include <reflex/serde/detail/io.hpp>

REFLEX_EXPORT namespace reflex::serde::csv
{
  // A CSV cell holds a single scalar value. Nested structs and containers have no
  // tabular representation and are rejected at compile time (see csv_row_c).
  template <typename T>
  concept csv_scalar_c = number_c<T>
                      or std::same_as<T, bool>
                      or std::same_as<T, char>
                      or enum_c<T>
                      or str_c<T>
                      or derives_c<T, derive_t<Format>>;

  namespace detail
  {
  template <typename T> struct is_optional : std::false_type
  {};
  template <typename T> struct is_optional<std::optional<T>> : std::true_type
  {};

  template <typename T> struct field_value
  {
    using type = T;
  };
  template <typename T> struct field_value<std::optional<T>>
  {
    using type = T;
  };
  } // namespace detail

  // A field is a scalar or an optional scalar.
  template <typename T>
  concept csv_field_c = csv_scalar_c<typename detail::field_value<std::remove_cvref_t<T>>::type>;

  namespace detail
  {
  template <typename T> consteval bool row_fields_ok()
  {
    bool ok = true;
    template for(constexpr auto member : define_static_array(
                     nonstatic_data_members_of(^^T, std::meta::access_context::current())))
    {
      using F = std::remove_cvref_t<typename[:type_of(member):]>;
      if constexpr(not csv_field_c<F>)
      {
        ok = false;
      }
    }
    return ok;
  }
  } // namespace detail

  // A row is a flat aggregate whose every member is a CSV field.
  template <typename T>
  concept csv_row_c = aggregate_c<T> and not str_c<T> and not seq_c<T>
                  and detail::row_fields_ok<std::remove_cvref_t<T>>();

  namespace detail
  {
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
      static_assert(false, std::string(display_string_of(^^F)) + " is not a CSV scalar");
    }
  }

  // RFC 4180 quoting: quote the cell only when it contains a delimiter, quote, or
  // line ending, and double any embedded quote.
  template <typename OutputIt> void write_escaped(OutputIt& out, std::string_view cell)
  {
    if(cell.find_first_of(",\"\r\n") == std::string_view::npos)
    {
      for(char c : cell) out++ = c;
      return;
    }
    out++ = '"';
    for(char c : cell)
    {
      if(c == '"') out++ = '"';
      out++ = c;
    }
    out++ = '"';
  }

  template <typename OutputIt, typename F> void write_field(OutputIt& out, F const& value)
  {
    if constexpr(is_optional<F>::value)
    {
      if(value.has_value()) write_escaped(out, field_text(*value));
      // an empty optional is an empty cell
    }
    else
    {
      write_escaped(out, field_text(value));
    }
  }

  template <csv_row_c Row, typename OutputIt> void write_header(OutputIt& out)
  {
    bool first = true;
    template for(constexpr auto member : define_static_array(
                     nonstatic_data_members_of(^^Row, std::meta::access_context::current())))
    {
      if(not first) out++ = ',';
      else first = false;
      constexpr std::string_view name = serialized_name(member);
      write_escaped(out, name);
    }
    out++ = '\r';
    out++ = '\n';
  }

  template <typename OutputIt, csv_row_c Row> void write_row(OutputIt& out, Row const& row)
  {
    bool first = true;
    template for(constexpr auto member : define_static_array(
                     nonstatic_data_members_of(^^Row, std::meta::access_context::current())))
    {
      if(not first) out++ = ',';
      else first = false;
      write_field(out, row.[:member:]);
    }
    out++ = '\r';
    out++ = '\n';
  }

  template <typename F> F parse_field(std::string_view cell)
  {
    if constexpr(is_optional<F>::value)
    {
      using U = typename field_value<F>::type;
      if(cell.empty()) return std::nullopt;
      return parse_field<U>(cell);
    }
    else if constexpr(array_of_c<F>) // std::array<char, N>
    {
      F          arr{};
      const auto n = std::min(cell.size(), arr.size());
      std::ranges::copy_n(cell.begin(), n, arr.begin());
      return arr;
    }
    else if constexpr(str_c<F>)
    {
      return F{cell};
    }
    else if constexpr(std::same_as<F, bool>)
    {
      return cell == "true" or cell == "1";
    }
    else if constexpr(std::same_as<F, char>)
    {
      return cell.empty() ? char{} : cell.front();
    }
    else if constexpr(number_c<F>)
    {
      F value{};
      auto [ptr, ec] = std::from_chars(cell.data(), cell.data() + cell.size(), value);
      if(ec != std::errc{}) throw std::runtime_error("CSV: failed to parse number");
      return value;
    }
    else if constexpr(derives_c<F, derive_t<Parse>>)
    {
      return parse_or_throw<F>(cell);
    }
    else if constexpr(enum_c<F>)
    {
      std::underlying_type_t<F> value{};
      auto [ptr, ec] = std::from_chars(cell.data(), cell.data() + cell.size(), value);
      if(ec != std::errc{}) throw std::runtime_error("CSV: failed to parse enum");
      return static_cast<F>(value);
    }
    else
    {
      static_assert(false, std::string(display_string_of(^^F)) + " is not parseable from CSV");
    }
  }

  template <csv_row_c Row>
  void assign_row(
      Row&                            row,
      std::span<const std::string>    header,
      std::span<const std::string>    cells)
  {
    for(std::size_t i = 0; i < header.size() and i < cells.size(); ++i)
    {
      const std::string_view col  = header[i];
      const std::string_view cell = cells[i];
      bool                   matched = false;
      template for(constexpr auto member : define_static_array(
                       nonstatic_data_members_of(^^Row, std::meta::access_context::current())))
      {
        if(not matched and (identifier_of(member) == col or serialized_name(member) == col))
        {
          using F      = std::remove_cvref_t<decltype(row.[:member:])>;
          row.[:member:] = parse_field<F>(cell);
          matched      = true;
        }
      }
      // unknown columns are ignored
    }
  }
  } // namespace detail

  template <typename OutputIt> class serializer : public serde::detail::serializer_base<OutputIt>
  {
  public:
    using serde::detail::serializer_base<OutputIt>::serializer_base;

    static constexpr std::string_view format_name = "CSV";
    static constexpr std::string_view format_hint =
        "(expected a flat aggregate or a sequence of flat aggregates)";
  };

  template <typename... TArgs>
  serializer(std::basic_string<TArgs...> & out)
      -> serializer<std::back_insert_iterator<std::basic_string<TArgs...>>>;
  serializer(std::ofstream & out) -> serializer<std::ostreambuf_iterator<char>>;
  serializer(std::ostringstream & out) -> serializer<std::ostreambuf_iterator<char>>;

  template <typename OutputIt, seq_c Seq>
    requires csv_row_c<std::ranges::range_value_t<Seq>>
  OutputIt tag_invoke(tag_t<serde::serialize>, serializer<OutputIt> & ser, Seq const& value)
  {
    auto& out = ser.out();
    detail::write_header<std::ranges::range_value_t<Seq>>(out);
    for(auto const& row : value) detail::write_row(out, row);
    return out;
  }

  template <typename OutputIt, csv_row_c Row>
  OutputIt tag_invoke(tag_t<serde::serialize>, serializer<OutputIt> & ser, Row const& value)
  {
    auto& out = ser.out();
    detail::write_header<Row>(out);
    detail::write_row(out, value);
    return out;
  }

  template <std::input_iterator InputIt>
  class deserializer : public serde::detail::subrange_deserializer<InputIt>
  {
    using base = serde::detail::subrange_deserializer<InputIt>;
    using base::cursor_;

  public:
    using base::base;
    using base::at_end;

    char peek() const
    {
      return *cursor_.begin();
    }

    char advance()
    {
      const char c = peek();
      cursor_.advance(1);
      return c;
    }

    // Read one RFC 4180 record into its fields, or nullopt at end of input.
    // Quoted fields may span embedded commas, quotes (doubled), and line breaks.
    std::optional<std::vector<std::string>> read_record()
    {
      if(at_end()) return std::nullopt;

      std::vector<std::string> fields;
      std::string              field;
      bool                     in_quotes = false;

      while(not at_end())
      {
        const char c = advance();
        if(in_quotes)
        {
          if(c == '"')
          {
            if(not at_end() and peek() == '"')
            {
              field.push_back('"');
              advance();
            }
            else
            {
              in_quotes = false;
            }
          }
          else
          {
            field.push_back(c);
          }
        }
        else if(c == '"')
        {
          in_quotes = true;
        }
        else if(c == ',')
        {
          fields.push_back(std::move(field));
          field.clear();
        }
        else if(c == '\r')
        {
          if(not at_end() and peek() == '\n') advance();
          break;
        }
        else if(c == '\n')
        {
          break;
        }
        else
        {
          field.push_back(c);
        }
      }

      fields.push_back(std::move(field));
      return fields;
    }

    template <typename T> T load()
    {
      return deserialize(*this, std::type_identity<T>{});
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

  template <typename InputIt, seq_c Seq>
    requires csv_row_c<std::ranges::range_value_t<Seq>>
  Seq tag_invoke(tag_t<serde::deserialize>, deserializer<InputIt> & de, std::type_identity<Seq>)
  {
    using Row = std::ranges::range_value_t<Seq>;
    Seq  result{};
    auto header = de.read_record();
    if(not header) return result;

    while(auto record = de.read_record())
    {
      // tolerate a blank trailing line (a single empty field)
      if(record->size() == 1 and record->front().empty()) continue;
      Row row{};
      detail::assign_row(row, *header, *record);
      result.push_back(std::move(row));
    }
    return result;
  }

  template <typename InputIt, csv_row_c Row>
  Row tag_invoke(tag_t<serde::deserialize>, deserializer<InputIt> & de, std::type_identity<Row>)
  {
    auto header = de.read_record();
    if(not header) throw std::runtime_error("CSV: missing header row");
    auto record = de.read_record();
    if(not record) throw std::runtime_error("CSV: missing data row");

    Row row{};
    detail::assign_row(row, *header, *record);
    return row;
  }

} // namespace reflex::serde::csv

REFLEX_EXPORT namespace reflex::serde::ser
{
  constexpr auto csv = ^^reflex::serde::csv::serializer;
}

REFLEX_EXPORT namespace reflex::serde::de
{
  constexpr auto csv = ^^reflex::serde::csv::deserializer;
}
