#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <cstring>

#include <reflex/concepts.hpp>
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
  using serde::detail::field_value;
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
  // Only a Format-deriving type materializes a string: its formatter is the only
  // thing that can render it, and there is no way to aim that at the sink
  // without a buffer.
  template <typename F> std::string field_text(F const& value)
  {
    return std::format("{}", value);
  }

  // Does this cell have to be quoted at all? Two scans, picked by cell length,
  // because neither wins everywhere: find_first_of rescans the whole needle set
  // per input byte, so it starts cheap and grows linearly, while find_any is
  // four memchr passes, a fixed setup that then barely grows.
  inline bool needs_quoting(std::string_view cell)
  {
    if(cell.size() < 32)
    {
      return cell.find_first_of(",\"\r\n") != std::string_view::npos;
    }
    return serde::detail::find_any(cell, 0, ",\"\r\n") != std::string_view::npos;
  }

  // RFC 4180 quoting: quote the cell only when it contains a delimiter, quote, or
  // line ending, and double any embedded quote.
  template <typename Ser> void write_escaped(Ser& ser, std::string_view cell)
  {
    if(not needs_quoting(cell))
    {
      ser.write_raw(cell);
      return;
    }
    ser.write_char('"');
    // One needle over the remainder of the cell, and the bound is cell.size(), so
    // the doubling loop cannot restart the search from the front and go quadratic.
    std::size_t pos = 0;
    while(pos < cell.size())
    {
      const std::size_t n = cell.find('"', pos);
      if(n == std::string_view::npos)
      {
        ser.write_raw(cell.substr(pos));
        break;
      }
      ser.write_raw(cell.substr(pos, n - pos + 1)); // the run, quote included
      ser.write_char('"');                          // the doubling
      pos = n + 1;
    }
    ser.write_char('"');
  }

  // Branch order is load-bearing and must stay as written. number_c, then
  // derives_c<Format>, then enum_c: a Format-deriving enum has to render through
  // its own formatter, not as its underlying integer, or it does not read back.
  template <typename Ser, typename F> void write_field(Ser& ser, F const& value)
  {
    if constexpr(optional_c<F>)
    {
      if(value.has_value()) write_field(ser, *value);
      // an empty optional is an empty cell
    }
    else if constexpr(array_of_c<F>) // std::array<char, N>, trimmed at the first NUL
    {
      write_escaped(ser, std::string_view{value.data(), ::strnlen(value.data(), value.size())});
    }
    else if constexpr(str_c<F>)
    {
      write_escaped(ser, std::string_view{value});
    }
    else if constexpr(std::same_as<F, bool>)
    {
      ser.write_raw(value ? "true" : "false"); // never needs quoting
    }
    else if constexpr(std::same_as<F, char>)
    {
      write_escaped(ser, std::string_view{&value, 1});
    }
    else if constexpr(number_c<F>)
    {
      serde::detail::write_digits(ser, value); // never needs quoting
    }
    else if constexpr(derives_c<F, derive_t<Format>>)
    {
      write_escaped(ser, field_text(value));
    }
    else if constexpr(enum_c<F>)
    {
      serde::detail::write_digits(ser, std::to_underlying(value));
    }
    else
    {
      static_assert(false, std::string(display_string_of(^^F)) + " is not a CSV scalar");
    }
  }

  // Writes one CRLF-terminated record: `write_cell<member>(ser)` emits each cell, this handles
  // the comma separator and the line ending.
  template <csv_row_c Row, typename Ser, typename WriteCell>
  void write_record(Ser& ser, WriteCell write_cell)
  {
    bool first = true;
    template for(constexpr auto member : define_static_array(
                     nonstatic_data_members_of(^^Row, std::meta::access_context::current())))
    {
      if(not first) ser.write_char(',');
      else first = false;
      write_cell.template operator()<member>(ser);
    }
    ser.write_raw("\r\n");
  }

  template <csv_row_c Row, typename Ser> void write_header(Ser& ser)
  {
    write_record<Row>(ser, []<auto member>(Ser& s) {
      constexpr std::string_view name = serialized_name(member);
      write_escaped(s, name);
    });
  }

  template <typename Ser, csv_row_c Row> void write_row(Ser& ser, Row const& row)
  {
    write_record<Row>(ser, [&row]<auto member>(Ser& s) { write_field(s, row.[:member:]); });
  }

  template <typename F> F parse_field(std::string_view cell)
  {
    if constexpr(optional_c<F>)
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
      // Trailing garbage is accepted: "12abc" reads as 12. from_chars reports how
      // far it got and this deliberately ignores it, matching the XML backend.
      // Tightening it is a cross-backend decision.
      F          value{};
      const auto ec = std::from_chars(cell.data(), cell.data() + cell.size(), value).ec;
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
      // Trailing garbage is accepted here too, see the number_c arm above.
      const auto ec = std::from_chars(cell.data(), cell.data() + cell.size(), value).ec;
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
    detail::write_header<std::ranges::range_value_t<Seq>>(ser);
    for(auto const& row : value) detail::write_row(ser, row);
    return ser.out();
  }

  template <typename OutputIt, csv_row_c Row>
  OutputIt tag_invoke(tag_t<serde::serialize>, serializer<OutputIt> & ser, Row const& value)
  {
    detail::write_header<Row>(ser);
    detail::write_row(ser, value);
    return ser.out();
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

  };

  REFLEX_SERDE_DESERIALIZER_DEDUCTION_GUIDES(deserializer);

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
