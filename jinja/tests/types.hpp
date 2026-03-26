#pragma once

#include <reflex/jinja.hpp>

struct aggregate1
{
  int         a;
  std::string b;
};

struct aggregate2
{
  float      x;
  aggregate1 nested;
};

struct aggregate3
{
  double                  x;
  std::vector<aggregate1> nested_list;
};

struct aggregate4
{
  bool                      flag;
  std::optional<aggregate2> optional_nested;
};

struct aggregate5
{
  int                     x;
  std::vector<aggregate3> nested_list;
};

// namespace reflex::poly {
//     extern template class var<bool, int, double, std::string, jinja::expr::loop_info&>;
//     extern template class var<bool, int, double, std::string, jinja::expr::loop_info&, aggregate1&>;
//     extern template class var<bool, int, double, std::string, jinja::expr::loop_info&, aggregate2&>;
//     extern template class var<bool, int, double, std::string, jinja::expr::loop_info&, aggregate3&>;
// }

// namespace reflex::jinja {

//     extern template class expr::context<>;
//     extern template class expr::context<aggregate1&>;
//     extern template class expr::context<aggregate2&>;
//     extern template class expr::context<aggregate3&>;
// }
