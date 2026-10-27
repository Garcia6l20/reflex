#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <reflex/concepts.hpp>
#include <reflex/meta.hpp>

#include <cstddef>
#include <cstdint>
#include <meta>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>
#endif

#include <reflex/const_check.hpp>

REFLEX_EXPORT namespace reflex::serde
{
  /** @brief Comments attached to one node of a model.
   *
   * @p leading holds the comment lines above the node, one entry per line, without the comment
   * marker. @p trailing holds the comment on the node's own line. @p footer holds the lines after
   * the node's last child and before its end, and is only read on an aggregate's own block. A
   * blank line is an entry equal to serde::blank_line.
   */
  struct comment_block
  {
    std::vector<std::string> leading;
    std::string              trailing;
    std::vector<std::string> footer;

    bool operator==(comment_block const&) const = default;
  };

  /** @brief The entry standing for one blank line in `leading` or `footer`.
   *
   * No comment line can hold a line break, so it never collides with a comment, and a bare
   * comment marker reads as the empty string, which stays distinct.
   */
  inline constexpr std::string_view blank_line = "\n";

  /** @brief Layout measured on read, honoured by the commented dump.
   *
   * @p indent is 0 when nothing was detected, the writer then uses its options. @p final_newline
   * records whether the input ended with a line break.
   */
  struct layout
  {
    std::uint8_t indent        = 0;
    char         indent_char   = ' ';
    bool         final_newline = true;

    bool operator==(layout const&) const = default;
  };

  /** @brief An aggregate node's own block. @p doc is read on the root only. */
  struct node_block : comment_block
  {
    serde::layout doc;

    bool operator==(node_block const&) const = default;
  };

  /** @brief The name of the member holding an aggregate node's own block.
   *
   * A model member of that name is rejected when serde::comments_of first names the model.
   */
  inline constexpr std::string_view reserved_comment_member = "_self";

  namespace detail
  {
  /** @brief A type the backends write and read member by member, the one shape given a nested
   * comment node.
   */
  template <typename T>
  concept reflected_aggregate_c =
      aggregate_c<T> and not str_c<T> and not seq_c<T> and not array_of_c<T> and not map_c<T>;

  template <typename T> struct comment_holder;

  template <typename T> struct comment_node
  {
    using type = comment_block;
  };

  template <reflected_aggregate_c T> struct comment_node<T>
  {
    using type = typename comment_holder<T>::type;
  };

  template <typename T> struct comment_node<std::optional<T>> : comment_node<T>
  {};

  template <typename T> using comment_node_t = typename comment_node<T>::type;

  template <typename T> consteval std::vector<std::meta::info> comment_member_specs()
  {
    std::vector<std::meta::info> specs{std::meta::data_member_spec(
        ^^node_block, {
                          .name = reserved_comment_member})};
    for(auto m : nonstatic_data_members_of(^^T, std::meta::access_context::current()))
    {
      if(not has_identifier(m))
      {
        continue;
      }
      REFLEX_META_CHECK(
          identifier_of(m) != reserved_comment_member,
          "a member named _self cannot have comments: serde::comments_of reserves the name for "
          "the node's own block, rename the member",
          m);
      specs.push_back(
          std::meta::data_member_spec(
              dealias(substitute(
                  ^^comment_node_t,
                  {
                      remove_cvref(type_of(m))})),
              {.name = identifier_of(m)}));
    }
    return specs;
  }

  /** @brief Holder of the comment tree generated for @p T.
   *
   * A member class of a class template, completed from the template's own consteval block,
   * for the reason given on cli::command_of: define_aggregate cannot complete a namespace-scope
   * type from inside a function, and two same-named local classes have their member
   * reflections interchanged under GCC 16.
   */
  template <typename T> struct comment_holder
  {
    struct type;
    consteval
    {
      std::meta::define_aggregate(^^type, comment_member_specs<T>());
    }
  };

  consteval bool is_comment_tree(std::meta::info t)
  {
    if(not is_class_type(t) or is_const_type(t) or is_volatile_type(t))
    {
      return false;
    }
    const auto holder = parent_of(dealias(t));
    return is_type(holder)
       and has_template_arguments(holder)
       and template_of(holder)
        == ^^comment_holder;
  }

  /** @brief A generated comment tree, the node of a reflected aggregate. */
  template <typename N>
  concept comment_tree_c = is_comment_tree(^^N);

  /** @brief Compares two comment trees member by member, `_self` included.
   *
   * Declared in the namespace of serde::detail::comment_holder, so argument-dependent lookup
   * finds it for every generated tree, and `!=` is synthesized from it.
   *
   * @return true when every non-static data member of @p lhs equals the one of @p rhs
   */
  template <comment_tree_c Node> constexpr bool operator==(Node const& lhs, Node const& rhs)
  {
    template for(constexpr auto m : std::define_static_array(
                     nonstatic_data_members_of(^^Node, std::meta::access_context::unchecked())))
    {
      if(not(lhs.[:m:] == rhs.[:m:]))
      {
        return false;
      }
    }
    return true;
  }
  } // namespace detail

  /** @brief The comment tree of @p T, members named after @p T's C++ members.
   *
   * A reflected aggregate gets a generated aggregate holding one serde::node_block named `_self`,
   * then one node per public non-static data member, named after the member's identifier and
   * never its serialized name. A nested reflected aggregate gets its own tree, `std::optional<U>`
   * gets the node of `U`, and every other type gets a serde::comment_block.
   */
  template <typename T> using comments_of = typename detail::comment_node<T>::type;

  namespace detail
  {
  /** @brief The node member of `comments_of<parent_of(Member)>` mirroring the model member
   * @p Member.
   */
  template <std::meta::info Member> consteval std::meta::info comment_member_of()
  {
    constexpr auto tree = std::meta::dealias(^^comments_of<typename[:parent_of(Member):]>);
    const auto     node =
        meta::member_named(tree, identifier_of(Member), std::meta::access_context::unchecked());
    REFLEX_META_CHECK(node != meta::null, "this member has no comment node", Member);
    return node;
  }
  } // namespace detail

  /** @brief The block holding @p block's own comments, @p block itself. */
  constexpr comment_block& comment(comment_block & block)
  {
    return block;
  }

  /** @brief The block holding @p block's own comments, @p block itself. */
  constexpr comment_block const& comment(comment_block const& block)
  {
    return block;
  }

  /** @brief The block holding the own comments of the aggregate node @p node. */
  template <detail::comment_tree_c Node> constexpr comment_block& comment(Node & node)
  {
    return node._self;
  }

  /** @brief The block holding the own comments of the aggregate node @p node. */
  template <detail::comment_tree_c Node> constexpr comment_block const& comment(Node const& node)
  {
    return node._self;
  }

  /** @brief The layout stored in the aggregate node @p node, meaningful on the root only. */
  template <detail::comment_tree_c Node> constexpr serde::layout& layout_of(Node & node)
  {
    return node._self.doc;
  }

  /** @brief The layout stored in the aggregate node @p node, meaningful on the root only. */
  template <detail::comment_tree_c Node> constexpr serde::layout const& layout_of(Node const& node)
  {
    return node._self.doc;
  }

  /** @brief A loaded value and the comments read with it. */
  template <typename T> struct commented
  {
    T              value;
    comments_of<T> comments;
  };

  namespace detail
  {
  template <typename N> inline constexpr char comment_node_tag = 0;

  /** @brief One node handed from a parent reader or writer to the next value's, tagged with
   * its type.
   *
   * A take of the wrong type yields null, and every take clears the slot, so a value read
   * through a user override never leaves a node for an unrelated nested reader.
   */
  template <typename Ptr> struct basic_comment_slot
  {
    Ptr         node = nullptr;
    void const* tag  = nullptr;

    template <typename N> constexpr void give(N& n)
    {
      node = &n;
      tag  = &comment_node_tag<std::remove_const_t<N>>;
    }

    template <typename N> constexpr N* take()
    {
      const bool match = tag == &comment_node_tag<std::remove_const_t<N>>;
      Ptr        p     = node;
      node             = nullptr;
      tag              = nullptr;
      if(not match)
      {
        return nullptr;
      }
      return static_cast<N*>(p);
    }
  };

  using comment_slot       = basic_comment_slot<void*>;
  using const_comment_slot = basic_comment_slot<void const*>;

  /** @brief A comment read but not yet attached to a node. */
  struct pending_comment
  {
    std::string text;
    std::size_t column    = 0;
    bool        same_line = false;
  };

  /** @brief A deserializer's capture state, held when its read options accept comments.
   *
   * @p active is set by load_commented only, so a plain load never stores a comment. @p root
   * stays true until the first aggregate reader takes the slot. @p line_column is the column
   * of the first non-space byte after the last line break crossed.
   */
  struct comment_capture
  {
    comment_slot                 slot;
    std::vector<pending_comment> pending;
    std::size_t                  line_column = 0;
    bool                         active      = false;
    bool                         root        = false;
  };

  /** @brief A serializer's emit state, held when its options write comments. */
  struct comment_emit
  {
    const_comment_slot slot;
    bool               active = false;
  };

  struct no_comment_state
  {};

  /** @brief @p raw without one leading space and without trailing spaces, tabs and `\r`. */
  constexpr std::string_view comment_body(std::string_view raw)
  {
    if(raw.starts_with(' '))
    {
      raw.remove_prefix(1);
    }
    const auto last = raw.find_last_not_of(" \t\r");
    return last == std::string_view::npos ? std::string_view{} : raw.substr(0, last + 1);
  }

  /** @brief Throws when @p text cannot be written as one comment line.
   *
   * @param text the comment body, never serde::blank_line, which a writer emits as an empty line
   * @param forbidden a sequence the format cannot hold inside a comment, empty for none
   * @param format the format named in the message
   * @throws std::runtime_error when @p text holds a line break or @p forbidden
   */
  inline void
      check_comment_line(std::string_view text, std::string_view forbidden, std::string_view format)
  {
    if(text.find_first_of("\r\n") != std::string_view::npos)
    {
      throw std::runtime_error(
          std::string(format)
          + ": a comment cannot hold a line break: '"
          + std::string(text)
          + "'");
    }
    if(not forbidden.empty() and text.find(forbidden) != std::string_view::npos)
    {
      throw std::runtime_error(
          std::string(format)
          + ": a comment cannot hold '"
          + std::string(forbidden)
          + "': '"
          + std::string(text)
          + "'");
    }
  }
  } // namespace detail
}
