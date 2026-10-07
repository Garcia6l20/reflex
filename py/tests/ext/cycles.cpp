#include <reflex/py.hpp>

#include <nanobind/stl/string.h>

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace py = reflex::py;
namespace nb = py::nb;

namespace
{
  template <typename Tag> struct counted
  {
    static inline int alive = 0;

    counted()
    {
      ++alive;
    }
    counted(counted const&)
    {
      ++alive;
    }
    ~counted()
    {
      --alive;
    }
  };

  struct holder
  {
    nb::object held = nb::none();

    [[= py::skip]] counted<holder> count;

    static auto alive() -> int
    {
      return counted<holder>::alive;
    }
  };

  struct tracked
  {
    [[= py::traverse]] nb::object held = nb::none();

    [[= py::skip]] counted<tracked> count;

    static auto alive() -> int
    {
      return counted<tracked>::alive;
    }
  };

  struct derived_tracked : tracked
  {
    [[= py::traverse]] nb::object other = nb::none();
  };

  struct containers
  {
    void push(nb::object o)
    {
      items.push_back(std::move(o));
    }

    void name(std::string const& key, nb::object o)
    {
      named.insert_or_assign(key, std::move(o));
    }

    void set_maybe(nb::object o)
    {
      maybe = std::move(o);
    }

  private:
    [[= py::traverse]] std::vector<nb::object>           items;
    [[= py::traverse]] std::map<std::string, nb::object> named;
    [[= py::traverse]] std::optional<nb::object>         maybe;
  };

  class hooked
  {
  public:
    void keep(nb::object o)
    {
      kept_.push_back(std::move(o));
    }

    auto size() const -> std::size_t
    {
      return kept_.size();
    }

    static auto alive() -> int
    {
      return counted<hooked>::alive;
    }

    static auto cleared() -> int
    {
      return clears;
    }

  private:
    [[= py::traverse]] void report(py::gc_visitor& visit) const noexcept
    {
      for(auto const& o : kept_)
      {
        visit(nb::handle{o});
      }
    }

    [[= py::clear]] void forget() noexcept
    {
      ++clears;
      auto dropped = std::move(kept_);
      kept_.clear();
    }

    std::vector<nb::object> kept_;
    counted<hooked>         count;

    static inline int clears = 0;
  };
}

namespace
{
  struct inner
  {
    [[= py::traverse]] nb::object held = nb::none();
  };

  struct outer
  {
    inner part;
  };

  struct nested
  {
    void put(std::string const& key, nb::object o)
    {
      slots[key].emplace_back(std::move(o));
    }

    void put_empty(std::string const& key)
    {
      slots[key].emplace_back(std::nullopt);
    }

    void key(nb::object o)
    {
      keyed.emplace_back(std::move(o), 0);
    }

  private:
    [[= py::traverse]] std::map<std::string, std::vector<std::optional<nb::object>>> slots;
    [[= py::traverse]] std::vector<std::pair<nb::object, int>>                         keyed;
  };

  struct listed
  {
    [[= py::traverse]] nb::list items;
  };

  class borrowing
  {
    [[= py::traverse]] void report(py::gc_visitor& visit) const noexcept
    {
      visit(seen_);
    }

    std::map<std::string, nb::handle> seen_;
  };

  static_assert(py::detail::holds_python_v<nb::object>);
  static_assert(py::detail::holds_python_v<nb::list>);
  static_assert(py::detail::holds_python_v<std::optional<nb::object>>);
  static_assert(py::detail::holds_python_v<std::map<std::string, std::vector<nb::object>>>);
  static_assert(py::detail::holds_python_v<std::vector<std::pair<nb::object, int>>>);
  static_assert(not py::detail::holds_python_v<nb::handle>);
  static_assert(not py::detail::holds_python_v<std::vector<nb::handle>>);
  static_assert(py::detail::holds_handle_v<std::map<std::string, nb::handle>>);
}

REFLEX_PY_MODULE(cycles, m)
{
  m.bind<holder>();
  m.bind<tracked>();
  m.bind<derived_tracked>();
  m.bind<containers>();
  m.bind<hooked>();
  m.bind<inner>();
  m.bind<outer>();
  m.bind<nested>();
  m.bind<listed>();
  m.bind<borrowing>();
}
