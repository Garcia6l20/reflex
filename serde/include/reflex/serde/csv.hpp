#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <array>
#include <cstring>
#include <deque>

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

  // `borrowed` says whether `cell` is a run of the deserializer's input rather
  // than of the pool read_record decodes into. Only a borrowed destination looks
  // at it, and the caller is the one that can answer it.
  template <typename F> F parse_field(std::string_view cell, bool borrowed = false)
  {
    if constexpr(optional_c<F>)
    {
      using U = typename field_value<F>::type;
      if(cell.empty()) return std::nullopt;
      return parse_field<U>(cell, borrowed);
    }
    else if constexpr(array_of_c<F>) // std::array<char, N>
    {
      F          arr{};
      const auto n = std::min(cell.size(), arr.size());
      std::ranges::copy_n(cell.begin(), n, arr.begin());
      return arr;
    }
    else if constexpr(serde::detail::string_sink_c<F>)
    {
      return F{cell};
    }
    else if constexpr(serde::detail::borrowed_string_sink_c<F>)
    {
      // A std::string_view member is handed a view of the input rather than a
      // copy, and choosing that member type is the opt-in.
      //
      // LIFETIME: the view is valid only while the input the deserializer was
      // given stays alive and unmodified. Nothing in the type system enforces
      // that, so `csv::deserializer{std::string{...}}.load<T>()` leaves every
      // borrowed member dangling. Deserialize from an lvalue that outlives the
      // result, or from serde::mmap_input_stream.
      //
      // A cell that needed decoding, which for CSV means a doubled quote, is not
      // a run of the input. read_record parks those in a pool it clears on the
      // next record, so handing out a view of one would go stale a record later
      // rather than at any boundary the caller can see. It throws instead.
      // Whether a cell is quoted is the producer's choice rather than the
      // schema's, so this is the caller's risk to take, deliberately.
      if(not borrowed and not cell.empty())
      {
        throw std::runtime_error("CSV: a borrowed string destination cannot hold a decoded cell");
      }
      return F{cell};
    }
    else if constexpr(str_c<F>)
    {
      static_assert(
          false,
          std::string(display_string_of(dealias(^^F)))
              + " cannot be a CSV string destination: it neither owns writable storage nor can"
                " be pointed at a run of the input (expected std::string,"
                " reflex::heapless::string<N>, std::array<char, N> or std::string_view)");
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
      if(ec != std::errc{} or ptr != cell.data() + cell.size())
      {
        throw std::runtime_error("CSV: failed to parse number");
      }
      return value;
    }
    else if constexpr(derives_c<F, derive_t<Parse>>)
    {
      return parse_strict_or_throw<F>(cell);
    }
    else if constexpr(enum_c<F>)
    {
      std::underlying_type_t<F> value{};
      auto [ptr, ec] = std::from_chars(cell.data(), cell.data() + cell.size(), value);
      if(ec != std::errc{} or ptr != cell.data() + cell.size())
      {
        throw std::runtime_error("CSV: failed to parse enum");
      }
      return static_cast<F>(value);
    }
    else
    {
      static_assert(false, std::string(display_string_of(^^F)) + " is not parseable from CSV");
    }
  }

  template <typename T> consteval std::size_t member_count()
  {
    return nonstatic_data_members_of(^^T, std::meta::access_context::current()).size();
  }

  // Which header column each member takes its value from, or -1 for a member the
  // header does not mention. Built once per document.
  template <csv_row_c Row> using header_map = std::array<int, member_count<Row>()>;

  // The precedence here is the one the per-row scan had, and it is not the
  // obvious one. Columns are walked in order and each takes the first member
  // whose identifier or serialized name equals it, so when a header names the
  // same member twice the later column overwrites the earlier. Building the
  // table member-first instead would keep the earlier one, which is why this
  // loops over columns and lets the assignment overwrite.
  template <csv_row_c Row, typename Cell>
  header_map<Row> resolve_header(std::span<const Cell> header)
  {
    header_map<Row> map;
    map.fill(-1);
    for(std::size_t i = 0; i < header.size(); ++i)
    {
      const std::string_view col     = header[i];
      std::size_t            idx     = 0;
      bool                   matched = false;
      template for(constexpr auto member : define_static_array(
                       nonstatic_data_members_of(^^Row, std::meta::access_context::current())))
      {
        if(not matched and serialized_name(member) == col)
        {
          map[idx] = static_cast<int>(i);
          matched  = true;
        }
        ++idx;
      }
      // unknown columns are ignored
    }
    return map;
  }

  // Whether a cell's bytes are the input's. An un-decoded cell is a run of the
  // input and stays valid as long as it does. A decoded one points into the
  // deserializer's pool, which the next read_record clears, so it is not
  // borrowable however long the input lives. Always false on the streaming
  // cursor, where every cell is accumulated into a string of its own.
  template <typename De> bool borrows_cell(De const& de, std::string_view cell)
  {
    if constexpr(De::bulk_scan)
    {
      return de.borrows_input(cell);
    }
    else
    {
      return false;
    }
  }

  // One integer compare per member per row. The cells span is what read_record
  // hands out, which on contiguous input is a view into the input buffer, so
  // Cell is deduced.
  //
  // The deserializer comes along because a cell that read_record had to decode
  // points into its pool rather than into the input, and a borrowed destination
  // has to be able to tell the two apart.
  template <csv_row_c Row, typename Cell, typename De>
  void assign_row(Row& row, header_map<Row> const& map, std::span<const Cell> cells, De const& de)
  {
    std::size_t idx = 0;
    template for(constexpr auto member : define_static_array(
                     nonstatic_data_members_of(^^Row, std::meta::access_context::current())))
    {
      const int col = map[idx];
      // The bound is per record, not per document: a short row leaves the
      // members its missing cells would have filled at their defaults.
      if(col >= 0 and static_cast<std::size_t>(col) < cells.size())
      {
        using F = std::remove_cvref_t<decltype(row.[:member:])>;
        using U = typename field_value<F>::type;
        if constexpr(serde::detail::borrowed_string_sink_c<U>)
        {
          static_assert(
              De::bulk_scan,
              std::string(display_string_of(dealias(^^U)))
                  + " cannot be a CSV string destination on this cursor: a borrowed read needs a"
                    " contiguous input to point at, and this deserializer reads a character at a"
                    " time (use std::string, or deserialize from a contiguous input)");
        }
        const std::string_view cell{cells[static_cast<std::size_t>(col)]};
        row.[:member:] = parse_field<F>(cell, borrows_cell(de, cell));
      }
      ++idx;
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
    using base::bulk_scan;
    using base::rest;
    using base::skip;

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

    // Index of the first byte that can end a run of ordinary field content: a
    // delimiter, either line ending, or a quote. Everything before it is literal
    // and can be copied in one go.
    //
    // serde::detail::find_any cannot stop early here, the boundary is what it
    // scans for, so every pass runs to the end of the buffer and the parse turns
    // quadratic. The same call inside write_escaped is fine, its argument is one
    // cell.
    static std::size_t field_end(std::string_view sv)
    {
      static constexpr std::array<bool, 256> stops = [] {
        std::array<bool, 256> t{};
        t[static_cast<unsigned char>(',')]  = true;
        t[static_cast<unsigned char>('\r')] = true;
        t[static_cast<unsigned char>('\n')] = true;
        t[static_cast<unsigned char>('"')]  = true;
        return t;
      }();
      for(std::size_t i = 0; i < sv.size(); ++i)
      {
        if(stops[static_cast<unsigned char>(sv[i])]) return i;
      }
      return std::string_view::npos;
    }

    // One field of a record. On contiguous input a field is almost always a
    // single run of the input buffer, so it is handed out as a view of it. A
    // stream cursor has no buffer to point into and owns its fields instead.
    using field_str  = std::conditional_t<bulk_scan, std::string_view, std::string>;
    using field_list = std::vector<field_str>;

    // Read one RFC 4180 record into its fields, or nullopt at end of input.
    // Quoted fields may span embedded commas, quotes (doubled), and line breaks.
    //
    // The fields are views into the input buffer, valid until the next
    // read_record() on this deserializer and only while that buffer is alive. A
    // field that needed decoding points into a pool the next read_record() clears.
    // Copy a field to keep it past the record it came from.
    std::optional<std::span<const field_str>> read_record()
    {
      if(at_end()) return std::nullopt;

      fields_.clear();
      pool_.clear();

      std::string      scratch;             // stream mode accumulator
      std::string_view run;                 // borrowed run, null data means unset
      std::string*     owned = nullptr;     // set once the field had to be decoded
      bool             in_quotes = false;

      // Append to the field under construction. In borrowed mode this extends the
      // run while the new bytes are physically adjacent to it, so a field that never
      // needed decoding stays one view. The first non-adjacent append materializes.
      auto add = [&](std::string_view s) {
        if constexpr(bulk_scan)
        {
          if(owned != nullptr)
          {
            owned->append(s);
          }
          else if(run.data() == nullptr)
          {
            run = s;
          }
          else if(run.data() + run.size() == s.data())
          {
            run = std::string_view{run.data(), run.size() + s.size()};
          }
          else
          {
            // References into a deque survive its growth, which a vector's do not.
            owned = &pool_.emplace_back(run);
            owned->append(s);
          }
        }
        else
        {
          scratch.append(s);
        }
      };
      auto end_field = [&] {
        if constexpr(bulk_scan)
        {
          fields_.push_back(owned != nullptr ? std::string_view{*owned} : run);
          run   = std::string_view{};
          owned = nullptr;
        }
        else
        {
          fields_.push_back(std::move(scratch));
          scratch.clear();
        }
      };

      while(not at_end())
      {
        if constexpr(bulk_scan)
        {
          // The character loop below still handles every byte that means
          // something, so the lenient cases it implements (a quote opening
          // mid-field, content after a closing quote) are untouched: a run by
          // definition contains none of them.
          //
          // Both scans are bounded by what is then consumed, so the parse stays
          // linear. Neither restarts from the front of the field after a doubled
          // quote.
          const std::string_view sv = rest();
          const std::size_t      n =
              in_quotes ? sv.find('"') // inside quotes only a quote is special
                        : field_end(sv);
          const std::size_t len = (n == std::string_view::npos) ? sv.size() : n;
          if(len != 0)
          {
            add(sv.substr(0, len));
            skip(len);
            continue;
          }
        }
        // The address of the byte about to be consumed, so a single significant
        // byte can extend the borrowed run rather than break it.
        [[maybe_unused]] const char* at = nullptr;
        if constexpr(bulk_scan)
        {
          at = rest().data();
        }
        const char c = advance();
        if(in_quotes)
        {
          if(c == '"')
          {
            if(not at_end() and peek() == '"')
            {
              // The pair emits one quote, and it is the first of the two, which
              // is still adjacent to the run. The second is skipped, and that is
              // what breaks adjacency for whatever follows.
              if constexpr(bulk_scan) { add(std::string_view{at, 1}); }
              else { scratch.push_back('"'); }
              advance();
            }
            else
            {
              in_quotes = false;
            }
          }
          else
          {
            if constexpr(bulk_scan) { add(std::string_view{at, 1}); }
            else { scratch.push_back(c); }
          }
        }
        else if(c == '"')
        {
          in_quotes = true;
        }
        else if(c == ',')
        {
          end_field();
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
          if constexpr(bulk_scan) { add(std::string_view{at, 1}); }
          else { scratch.push_back(c); }
        }
      }

      end_field();
      return std::span<const field_str>{fields_};
    }

  private:
    field_list fields_;
    // Only the fields that needed decoding land here, so it stays empty on input
    // that has no doubled quotes. A deque rather than a vector because the views
    // handed out must survive the next emplace_back.
    std::deque<std::string> pool_;
  };

  REFLEX_SERDE_DESERIALIZER_DEDUCTION_GUIDES(deserializer);

  template <typename InputIt, seq_c Seq>
    requires csv_row_c<std::ranges::range_value_t<Seq>>
  Seq tag_invoke(tag_t<serde::deserialize>, deserializer<InputIt> & de, std::type_identity<Seq>)
  {
    using Row = std::ranges::range_value_t<Seq>;
    Seq  result{};
    auto header_rec = de.read_record();
    if(not header_rec) return result;
    // Resolved here, while the header record's fields are still valid. The map is
    // plain integers and borrows nothing, so the header itself no longer has to
    // be copied to outlive the records.
    const auto map = detail::resolve_header<Row>(*header_rec);

    while(auto record = de.read_record())
    {
      // tolerate a blank trailing line (a single empty field)
      if(record->size() == 1 and record->front().empty()) continue;
      Row row{};
      detail::assign_row(row, map, *record, de);
      result.push_back(std::move(row));
    }
    return result;
  }

  template <typename InputIt, csv_row_c Row>
  Row tag_invoke(tag_t<serde::deserialize>, deserializer<InputIt> & de, std::type_identity<Row>)
  {
    auto header_rec = de.read_record();
    if(not header_rec) throw std::runtime_error("CSV: missing header row");
    // Must be resolved before the next read_record, which invalidates the fields
    // it is built from.
    const auto map = detail::resolve_header<Row>(*header_rec);

    auto record = de.read_record();
    if(not record) throw std::runtime_error("CSV: missing data row");

    Row row{};
    detail::assign_row(row, map, *record, de);
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
