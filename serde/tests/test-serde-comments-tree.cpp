#include <doctest/doctest.h>

#include <reflex/const_check.hpp>

import reflex.serde.json;
import reflex.poly;

import std;

using namespace reflex;
using namespace std::string_view_literals;

struct ct_inner
{
  int         port;
  std::string host;
};

struct ct_outer
{
  int                        value;
  ct_inner                   srv;
  std::optional<ct_inner>    backup;
  std::vector<int>           list;
  std::map<std::string, int> tags;
};

struct ct_renamed
{
  [[= serde::rename{"Port"}]] int port;
};

struct ct_tree
{
  int                  v;
  std::vector<ct_tree> kids;
};

struct ct_dynamic
{
  poly::var<int, std::string> any;
  std::array<int, 2>          pair;
  ct_inner const              fixed;
};

struct ct_clash
{
  int _self;
};

using ct_outer_tree = serde::comments_of<ct_outer>;

static_assert(std::is_same_v<decltype(ct_outer_tree{}.value), serde::comment_block>);
static_assert(std::is_same_v<decltype(ct_outer_tree{}.list), serde::comment_block>);
static_assert(std::is_same_v<decltype(ct_outer_tree{}.tags), serde::comment_block>);
static_assert(std::is_same_v<decltype(ct_outer_tree{}.srv), serde::comments_of<ct_inner>>);
static_assert(std::is_same_v<decltype(ct_outer_tree{}.backup), serde::comments_of<ct_inner>>);
static_assert(std::is_same_v<serde::comments_of<int>, serde::comment_block>);
static_assert(std::is_same_v<serde::comments_of<std::string>, serde::comment_block>);
static_assert(std::is_same_v<serde::comments_of<std::optional<int>>, serde::comment_block>);
static_assert(
    std::is_same_v<serde::comments_of<std::optional<ct_inner>>, serde::comments_of<ct_inner>>);

static_assert(serde::detail::comment_tree_c<ct_outer_tree>);
static_assert(not serde::detail::comment_tree_c<ct_outer_tree const>);
static_assert(not serde::detail::comment_tree_c<ct_outer>);
static_assert(not serde::detail::comment_tree_c<serde::comment_block>);

consteval std::size_t ct_member_count(std::meta::info t)
{
  return nonstatic_data_members_of(t, std::meta::access_context::unchecked()).size();
}

consteval std::meta::info ct_first_member(std::meta::info t)
{
  return nonstatic_data_members_of(t, std::meta::access_context::unchecked())[0];
}

static_assert(ct_member_count(^^ct_outer_tree) == 6);
static_assert(identifier_of(ct_first_member(^^ct_outer_tree)) == serde::reserved_comment_member);
static_assert(type_of(ct_first_member(^^ct_outer_tree)) == ^^serde::node_block);
static_assert(ct_member_count(^^serde::comments_of<ct_inner>) == 3);

static_assert(serde::detail::comment_member_of<^^ct_outer::srv>() == ^^ct_outer_tree::srv);
static_assert(
    serde::detail::comment_member_of<^^ct_inner::host>() == ^^serde::comments_of<ct_inner>::host);
static_assert(
    serde::detail::comment_member_of<^^ct_renamed::port>()
    == ^^serde::comments_of<ct_renamed>::port);

static_assert(std::is_same_v<decltype(serde::comments_of<ct_tree>{}.kids), serde::comment_block>);
static_assert(std::is_same_v<decltype(serde::comments_of<ct_dynamic>{}.any), serde::comment_block>);
static_assert(
    std::is_same_v<decltype(serde::comments_of<ct_dynamic>{}.pair), serde::comment_block>);
static_assert(
    std::is_same_v<decltype(serde::comments_of<ct_dynamic>{}.fixed), serde::comments_of<ct_inner>>);

consteval
{
  REFLEX_CONSTEVAL_THROWS_WITH(
      "serde::comments_of reserves the name", serde::detail::comment_member_specs<ct_clash>());
}

template <typename Model> std::size_t ct_leading_of_first_use(Model const&)
{
  serde::comments_of<Model> tree;
  serde::comment(tree).leading.push_back("first");
  serde::comment(tree.srv).leading.push_back("nested");
  return serde::comment(tree).leading.size() + serde::comment(tree.srv).leading.size();
}

struct ct_first_use_inner
{
  bool on;
};

struct ct_first_use
{
  ct_first_use_inner srv;
};

TEST_CASE("reflex::serde::comments_of: first named inside a function template")
{
  CHECK_EQ(ct_leading_of_first_use(ct_first_use{}), 2u);
  static_assert(ct_member_count(^^serde::comments_of<ct_first_use>) == 2);
}

TEST_CASE("reflex::serde::comments_of: accessors")
{
  ct_outer_tree tree;

  CHECK_EQ(serde::blank_line, "\n"sv);
  CHECK_EQ(serde::layout_of(tree), serde::layout{});
  CHECK_EQ(serde::layout_of(tree).indent, 0);
  CHECK(serde::layout_of(tree).final_newline);

  serde::comment(tree).leading.push_back("header");
  serde::comment(tree).leading.push_back(std::string(serde::blank_line));
  serde::comment(tree.srv).trailing = "server";
  serde::comment(tree.value).leading.push_back("value");
  serde::layout_of(tree).indent = 4;

  CHECK_EQ(tree._self.leading, std::vector<std::string>{"header", "\n"});
  CHECK_EQ(tree.srv._self.trailing, "server");
  CHECK_EQ(&serde::comment(tree.value), &tree.value);
  CHECK_EQ(tree.value.leading, std::vector<std::string>{"value"});
  CHECK_EQ(tree._self.doc.indent, 4);

  ct_outer_tree const& view = tree;
  CHECK_EQ(&serde::comment(view), static_cast<serde::comment_block const*>(&tree._self));
  CHECK_EQ(serde::layout_of(view).indent, 4);
  static_assert(std::is_same_v<decltype(serde::comment(view)), serde::comment_block const&>);
  static_assert(std::is_same_v<decltype(serde::layout_of(view)), serde::layout const&>);

  serde::node_block copy = tree._self;
  CHECK_EQ(copy, tree._self);
  copy.doc.final_newline = false;
  CHECK_NE(copy, tree._self);
}

TEST_CASE("reflex::serde::detail::comment_slot")
{
  serde::comments_of<ct_inner> inner;
  ct_outer_tree                outer;
  serde::detail::comment_slot  slot;

  SUBCASE("a take of another type yields null and clears")
  {
    slot.give(inner);
    CHECK_EQ(slot.take<ct_outer_tree>(), nullptr);
    CHECK_EQ(slot.node, nullptr);
    CHECK_EQ(slot.tag, nullptr);
    CHECK_EQ(slot.take<serde::comments_of<ct_inner>>(), nullptr);
  }

  SUBCASE("a take of the given type yields it and clears")
  {
    slot.give(outer);
    CHECK_EQ(slot.take<ct_outer_tree>(), &outer);
    CHECK_EQ(slot.take<ct_outer_tree>(), nullptr);
  }

  SUBCASE("a const slot hands out a const node")
  {
    serde::detail::const_comment_slot const_slot;
    ct_outer_tree const&              view = outer;
    const_slot.give(view);
    CHECK_EQ(const_slot.take<ct_outer_tree const>(), &outer);
  }

  SUBCASE("an empty slot yields null")
  {
    CHECK_EQ(slot.take<serde::comment_block>(), nullptr);
  }
}

TEST_CASE("reflex::serde::detail::comment_body")
{
  CHECK_EQ(serde::detail::comment_body(" x "), "x"sv);
  CHECK_EQ(serde::detail::comment_body("  x\t\r"), " x"sv);
  CHECK_EQ(serde::detail::comment_body(""), ""sv);
  CHECK_EQ(serde::detail::comment_body(" "), ""sv);
  CHECK_EQ(serde::detail::comment_body("x y"), "x y"sv);
}

TEST_CASE("reflex::serde::detail::check_comment_line")
{
  CHECK_NOTHROW(serde::detail::check_comment_line("plain text", "", "JSON"));
  CHECK_NOTHROW(serde::detail::check_comment_line("", "--", "XML"));
  CHECK_THROWS_WITH_AS(
      serde::detail::check_comment_line("a\nb", "", "JSON"),
      "JSON: a comment cannot hold a line break: 'a\nb'", std::runtime_error);
  CHECK_THROWS_AS(serde::detail::check_comment_line("a\rb", "", "TOML"), std::runtime_error);
  CHECK_THROWS_WITH_AS(
      serde::detail::check_comment_line("a -- b", "--", "XML"),
      "XML: a comment cannot hold '--': 'a -- b'", std::runtime_error);
}

struct ct_read_options
{
  bool comments = true;
};

class ct_reader : public serde::detail::subrange_deserializer<char const*>
{
  using base = serde::detail::subrange_deserializer<char const*>;
  friend base;

  serde::detail::comment_capture comments_;

  template <typename Node> void begin_commented(Node& node)
  {
    calls.push_back("begin");
    serde::comment(node).leading.push_back("header");
    comments_.root = true;
  }

  template <typename Node> void end_commented(Node& node)
  {
    calls.push_back("end");
    serde::comment(node).footer.push_back("footer");
    serde::layout_of(node).indent = 2;
  }

public:
  static constexpr ct_read_options read_opts{};

  std::vector<std::string> calls;
  bool                     fail = false;

  using base::base;

  bool capturing() const
  {
    return comments_.active;
  }

  bool slot_empty() const
  {
    return comments_.slot.node == nullptr and comments_.slot.tag == nullptr;
  }

  template <typename T>
  friend T tag_invoke(tag_t<serde::deserialize>, ct_reader& de, std::type_identity<T>)
  {
    de.calls.push_back(de.comments_.active ? "load active" : "load inactive");
    if(de.fail)
    {
      throw std::runtime_error("ct_reader failed");
    }
    if(auto* node = de.comments_.slot.template take<serde::comments_of<T>>())
    {
      de.comments_.root    = false;
      node->value.trailing = "taken";
    }
    T value{};
    value.value = 7;
    return value;
  }
};

struct ct_write_options
{
  bool comments = true;
};

class ct_writer : public serde::detail::serializer_base<std::back_insert_iterator<std::string>>
{
  using base = serde::detail::serializer_base<std::back_insert_iterator<std::string>>;
  friend base;

  serde::detail::comment_emit comments_;

  template <typename Node> void begin_dump(Node const& node)
  {
    for(auto const& line : serde::comment(node).leading)
    {
      if(line != serde::blank_line)
      {
        write_raw("# ");
        write_raw(line);
      }
      write_char('\n');
    }
  }

  template <typename Node> void end_dump(Node const& node)
  {
    for(auto const& line : serde::comment(node).footer)
    {
      write_raw("\n# ");
      write_raw(line);
    }
    if(serde::layout_of(node).final_newline)
    {
      write_char('\n');
    }
  }

public:
  static constexpr ct_write_options opts{};

  using base::base;

  bool emitting() const
  {
    return comments_.active;
  }

  template <typename T>
  friend std::back_insert_iterator<std::string>
      tag_invoke(tag_t<serde::serialize>, ct_writer& ser, T const& value)
  {
    ser.write_raw(std::format("value = {}", value.value));
    if(auto const* node = ser.comments_.slot.template take<serde::comments_of<T> const>())
    {
      ser.write_raw(" # ");
      ser.write_raw(node->value.trailing);
    }
    return ser.out();
  }
};

TEST_CASE("reflex::serde::detail::subrange_deserializer::load_commented")
{
  constexpr std::string_view input = "unused";

  SUBCASE("runs the hooks around the load and hands the root node down")
  {
    ct_reader de{input.data(), input.data() + input.size()};
    CHECK_FALSE(de.capturing());

    auto [value, comments] = de.load_commented<ct_outer>();
    static_assert(std::is_same_v<decltype(comments), ct_outer_tree>);

    CHECK_EQ(value.value, 7);
    CHECK_EQ(de.calls, std::vector<std::string>{"begin", "load active", "end"});
    CHECK_EQ(serde::comment(comments).leading, std::vector<std::string>{"header"});
    CHECK_EQ(serde::comment(comments).footer, std::vector<std::string>{"footer"});
    CHECK_EQ(comments.value.trailing, "taken");
    CHECK_EQ(serde::layout_of(comments).indent, 2);
    CHECK_FALSE(de.capturing());
    CHECK(de.slot_empty());

    std::ignore = de.load<ct_outer>();
    CHECK_EQ(de.calls.back(), "load inactive");
  }

  SUBCASE("a throwing load leaves capture off and the slot empty")
  {
    ct_reader de{input.data(), input.data() + input.size()};
    de.fail = true;
    CHECK_THROWS_WITH_AS(
        std::ignore = de.load_commented<ct_outer>(), "ct_reader failed", std::runtime_error);
    CHECK_FALSE(de.capturing());
    CHECK(de.slot_empty());
  }
}

TEST_CASE("reflex::serde::detail::serializer_base::dump with comments")
{
  ct_outer_tree tree;
  serde::comment(tree).leading = {"header", std::string(serde::blank_line)};
  serde::comment(tree).footer  = {"footer"};
  tree.value.trailing          = "the value";

  ct_outer model{};
  model.value = 3;

  std::string out;
  ct_writer   ser{out};
  ser.dump(model, tree);
  CHECK_EQ(out, "# header\n\nvalue = 3 # the value\n# footer\n");
  CHECK_FALSE(ser.emitting());

  out.clear();
  model.value = 4;
  ser.dump(model);
  CHECK_EQ(out, "value = 4");
}
