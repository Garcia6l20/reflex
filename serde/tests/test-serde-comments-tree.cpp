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
