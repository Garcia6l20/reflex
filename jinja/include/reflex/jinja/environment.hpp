#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#include <reflex/jinja.hpp>

#ifndef REFLEX_MODULE
#include <reflex/scope_guard.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#endif

REFLEX_EXPORT namespace reflex::jinja
{
  // Resolves a template name to its source text, nullopt when the template does not exist.
  // A loader never throws on a miss, `environment::get` decides whether a miss is fatal.
  using loader = std::function<std::optional<std::string>(std::string_view name)>;

  // Loads templates from `root`. Names that would escape `root` (absolute, or climbing through
  // '..') are refused: template names reach this through {% include %}, hence from template text.
  inline loader filesystem_loader(std::filesystem::path root, std::string ext = ".jinja")
  {
    return [root = std::move(root),
            ext  = std::move(ext)](std::string_view name) -> std::optional<std::string> {
      auto relative = std::string{name};
      if(not ext.empty() and not relative.ends_with(ext))
      {
        relative += ext;
      }

      auto normalized = std::filesystem::path{relative}.lexically_normal();
      if(normalized.is_absolute() or normalized.native().starts_with(".."))
      {
        return std::nullopt;
      }

      auto            path = root / normalized;
      std::error_code ec;
      if(not std::filesystem::is_regular_file(path, ec))
      {
        return std::nullopt;
      }

      std::ifstream input{path, std::ios::binary};
      if(not input)
      {
        return std::nullopt;
      }

      return std::string{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    };
  }

  inline loader map_loader(std::map<std::string, std::string, std::less<>> sources)
  {
    return [sources = std::move(sources)](std::string_view name) -> std::optional<std::string> {
      if(auto it = sources.find(name); it != sources.end())
      {
        return it->second;
      }
      return std::nullopt;
    };
  }

  // Owns template sources and their parsed trees, and resolves templates by name through a
  // pluggable loader. Owning the sources is what makes the string_view slices stored in
  // `template_` safe beyond the caller's buffer.
  class environment
  {
  public:
    explicit environment(loader load) : load_{std::move(load)}
    {}

    // Parses and caches. The returned reference stays valid for the environment's lifetime.
    const template_& get(std::string_view name)
    {
      if(auto it = parsed_.find(name); it != parsed_.end())
      {
        return it->second;
      }

      auto source = load_(name);
      if(not source)
      {
        throw runtime_error("Template '{}' not found", name);
      }

      // std::map nodes never move, so the string_views the parsed tree holds into the stored
      // source stay valid. A failed parse must not leave the source behind: the next get()
      // would then reuse it and keep reporting the stale error.
      auto [source_it, _] = sources_.insert_or_assign(std::string{name}, std::move(*source));
      scope_guard rollback{[&] { sources_.erase(source_it); }};

      auto& parsed = parsed_.emplace(std::string{name}, parse(source_it->second)).first->second;
      rollback.disable();
      return parsed;
    }

    bool has(std::string_view name)
    {
      if(parsed_.contains(name))
      {
        return true;
      }
      return load_(name).has_value();
    }

    // Renders `tmpl` resolving {% extends %} and {% block %} against this environment.
    template <typename OutputIt, typename ContextT>
    OutputIt render_template_to(
        OutputIt              out,
        const template_&      tmpl,
        ContextT&             ctx,
        detail::render_state& state)
    {
      // Walk the inheritance chain from most-derived to root, keeping the first override seen
      // for each block name, and render the root base.
      const template_*              current = &tmpl;
      std::vector<std::string_view> seen;

      while(current->extends)
      {
        for(const auto& [name, body] : current->blocks)
        {
          if(detail::find_block(state.block_overrides, name) == nullptr)
          {
            state.block_overrides.emplace_back(name, body);
          }
        }

        auto base = *current->extends;
        if(std::ranges::contains(seen, base))
        {
          throw runtime_error("Cyclic template inheritance through '{}'", base);
        }
        seen.push_back(base);
        current = &get(base);
      }

      return detail::render_children_to(out, current->children, ctx, &state);
    }

    template <typename OutputIt, typename ContextT>
    OutputIt render_to(OutputIt out, std::string_view name, ContextT& ctx)
    {
      detail::render_state state{.env = this, .include_stack = {name}, .block_overrides = {}};
      return render_template_to(out, get(name), ctx, state);
    }

    template <typename ContextT> std::string render(std::string_view name, ContextT& ctx)
    {
      std::string result;
      render_to(std::back_inserter(result), name, ctx);
      return result;
    }

    // Renders a source string the environment takes a copy of, so the caller has no lifetime
    // constraint on it. Includes and inheritance resolve through this environment.
    template <typename ContextT> std::string render_source(std::string_view source, ContextT& ctx)
    {
      // Nothing outlives the call: the tree dies here and the output is a fresh string, so the
      // copy can stay local.
      const std::string owned{source};
      auto              tmpl = parse(owned);

      std::string          result;
      detail::render_state state{.env = this, .include_stack = {}, .block_overrides = {}};
      render_template_to(std::back_inserter(result), tmpl, ctx, state);
      return result;
    }

  private:
    loader load_;
    // Node-stable storage: the string_view slices in template_ point into these sources, and
    // std::map never moves a mapped value once inserted.
    std::map<std::string, std::string, std::less<>> sources_;
    std::map<std::string, template_, std::less<>>   parsed_;
  };

  namespace detail
  {

  template <typename OutputIt, typename ContextT>
  OutputIt
      render_include_to(OutputIt out, std::string_view name, ContextT& ctx, render_state& state)
  {
    if(std::ranges::contains(state.include_stack, name))
    {
      throw runtime_error("Cyclic include of template '{}'", name);
    }

    state.include_stack.push_back(name);

    // The includer's block overrides do not apply to the included template.
    render_state sub{.env = state.env, .include_stack = state.include_stack, .block_overrides = {}};
    out = state.env->render_template_to(out, state.env->get(name), ctx, sub);

    state.include_stack.pop_back();
    return out;
  }

  } // namespace detail
} // namespace reflex::jinja
