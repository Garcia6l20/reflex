#include <doctest/doctest.h>

import reflex.serde.yaml;

import std;

using namespace reflex;
using namespace reflex::serde;
using namespace std::literals;

struct yw_srv
{
  std::string host;
  int         port    = 0;
  int         timeout = 0;

  bool operator==(yw_srv const&) const = default;
};

struct yw_config
{
  std::string title;
  int         port  = 0;
  bool        debug = false;
  yw_srv      srv;
  int         retries = 0;

  bool operator==(yw_config const&) const = default;
};

struct yw_inner
{
  int port = 0;
};

struct yw_shape
{
  int      value = 0;
  yw_inner srv;
};

struct yw_empty
{};

struct yw_custom
{
  int x = 0;

  template <typename O, yaml::options P>
  friend O tag_invoke(tag_t<serde::serialize>, yaml::serializer<O, P>& ser, yw_custom const& v)
  {
    serialize(ser, v.x);
    return ser.out();
  }
};

struct yw_values
{
  std::vector<int>           list;
  std::vector<int>           none;
  std::optional<yw_srv>      missing;
  yw_empty                   empty;
  yw_custom                  custom;
  std::map<std::string, int> table;
};

constexpr char yw_config_data[] = {
#embed "fixtures/comments/yaml/config.yaml"
};
constexpr char yw_expected_edit_data[] = {
#embed "fixtures/comments/yaml/expected-edit.yaml"
};
constexpr char yw_config4_data[] = {
#embed "fixtures/comments/yaml/config-indent4.yaml"
};
constexpr char yw_expected_edit4_data[] = {
#embed "fixtures/comments/yaml/expected-edit-indent4.yaml"
};

constexpr std::string_view yw_config_yaml{yw_config_data, sizeof yw_config_data};
constexpr std::string_view yw_expected_edit_yaml{
    yw_expected_edit_data, sizeof yw_expected_edit_data};
constexpr std::string_view yw_config4_yaml{yw_config4_data, sizeof yw_config4_data};
constexpr std::string_view yw_expected_edit4_yaml{
    yw_expected_edit4_data, sizeof yw_expected_edit4_data};

constexpr yaml::read_options yw_keep{.comments = true};
constexpr yaml::options      yw_write{.comments = true};

template <typename T> static serde::commented<T> yw_load(std::string_view text)
{
  yaml::deserializer de{text, std::cw<yw_keep>};
  return de.load_commented<T>();
}

template <typename T> static std::string yw_dump(T const& value, serde::comments_of<T> const& c)
{
  std::string out;
  yaml::serializer{out, std::cw<yw_write>}.dump(value, c);
  return out;
}

static std::vector<std::string_view> yw_lines(std::string_view text)
{
  std::vector<std::string_view> out;
  for(auto line : std::views::split(text, '\n'))
  {
    out.emplace_back(line.begin(), line.end());
  }
  return out;
}

TEST_CASE("reflex::serde::yaml write comments: the config edit changes one line")
{
  auto [cfg, c] = yw_load<yw_config>(yw_config_yaml);
  CHECK_EQ(cfg.srv.port, 9000);
  cfg.srv.port   = 9100;
  const auto out = yw_dump(cfg, c);
  CHECK_EQ(out, yw_expected_edit_yaml);

  const auto before = yw_lines(yw_config_yaml);
  const auto after  = yw_lines(out);
  REQUIRE_EQ(before.size(), after.size());
  std::vector<std::size_t> changed;
  for(std::size_t i = 0; i < before.size(); ++i)
  {
    if(before[i] != after[i])
    {
      changed.push_back(i);
    }
  }
  REQUIRE_EQ(changed.size(), 1);
  CHECK_EQ(before[changed[0]], "  port: 9000 # upstream port");
  CHECK_EQ(after[changed[0]], "  port: 9100 # upstream port");
}

TEST_CASE("reflex::serde::yaml write comments: an unchanged round trip is byte-identical")
{
  const auto [cfg, c] = yw_load<yw_config>(yw_config_yaml);
  const auto out      = yw_dump(cfg, c);
  CHECK_EQ(out, yw_config_yaml);

  const auto [again, ac] = yw_load<yw_config>(out);
  CHECK(again == cfg);
  CHECK_EQ(yw_dump(again, ac), yw_config_yaml);
}

TEST_CASE("reflex::serde::yaml write comments: a 4-space input stays 4-space")
{
  auto [cfg, c] = yw_load<yw_config>(yw_config4_yaml);
  CHECK_EQ(layout_of(c).indent, 4);
  CHECK_EQ(yw_dump(cfg, c), yw_config4_yaml);
  cfg.srv.port = 9100;
  CHECK_EQ(yw_dump(cfg, c), yw_expected_edit4_yaml);

  std::string      plain;
  yaml::serializer ser{plain, std::cw<yw_write>};
  ser.dump(cfg, c);
  plain.clear();
  ser.dump(yw_inner{1});
  CHECK_EQ(plain, "port: 1");
  plain.clear();
  ser.dump(yw_shape{1, {2}});
  CHECK_EQ(plain, "value: 1\nsrv:\n  port: 2");
}

TEST_CASE("reflex::serde::yaml write comments: a hand-built tree gives the documented shape")
{
  comments_of<yw_shape> c;
  comment(c).leading      = {"header", std::string(blank_line)};
  c.value.leading         = {"leading of value"};
  c.value.trailing        = "trailing of value";
  comment(c.srv).leading  = {std::string(blank_line), "section comment"};
  comment(c.srv).trailing = "trailing of srv";
  comment(c.srv).footer   = {"footer of srv"};
  comment(c).footer       = {"footer of root"};

  const auto out = yw_dump(yw_shape{1, {8080}}, c);
  CHECK_EQ(
      out, "# header\n"
           "\n"
           "# leading of value\n"
           "value: 1 # trailing of value\n"
           "\n"
           "# section comment\n"
           "srv: # trailing of srv\n"
           "  port: 8080\n"
           "  # footer of srv\n"
           "# footer of root\n");

  const auto [v, back] = yw_load<yw_shape>(out);
  CHECK_EQ(comment(back).leading, comment(c).leading);
  CHECK_EQ(comment(back).footer, comment(c).footer);
  CHECK_EQ(back.value, c.value);
  CHECK_EQ(comment(back.srv), comment(c.srv));
  CHECK_EQ(back.srv.port, c.srv.port);
}

TEST_CASE("reflex::serde::yaml write comments: an empty comment is a bare marker")
{
  comments_of<yw_inner> c;
  c.port.leading             = {""};
  c.port.trailing            = "";
  comment(c).footer          = {"", std::string(blank_line)};
  layout_of(c).final_newline = false;
  CHECK_EQ(yw_dump(yw_inner{1}, c), "#\nport: 1\n#\n");
}

TEST_CASE("reflex::serde::yaml write comments: trailing comments on block, empty and custom values")
{
  comments_of<yw_values> c;
  c.list.trailing             = "list";
  c.none.trailing             = "none";
  comment(c.missing).trailing = "missing";
  comment(c.empty).trailing   = "empty";
  comment(c.custom).leading   = {"custom lead"};
  comment(c.custom).trailing  = "custom";
  c.table.trailing            = "table";
  layout_of(c).final_newline  = false;

  yw_values v;
  v.list   = {1, 2};
  v.custom = {5};
  v.table  = {
      {"k", 1}
  };
  CHECK_EQ(
      yw_dump(v, c), "list: # list\n"
                     "  - 1\n"
                     "  - 2\n"
                     "none: [] # none\n"
                     "missing: null # missing\n"
                     "empty: {} # empty\n"
                     "# custom lead\n"
                     "custom: # custom\n"
                     "  5\n"
                     "table: # table\n"
                     "  k: 1");
}

TEST_CASE("reflex::serde::yaml write comments: a comment holding a line break throws")
{
  comments_of<yw_inner> c;
  c.port.trailing = "two\nlines";
  CHECK_THROWS_WITH_AS(
      std::ignore = yw_dump(yw_inner{1}, c),
      "YAML: a comment cannot hold a line break: 'two\nlines'", std::runtime_error);
}
