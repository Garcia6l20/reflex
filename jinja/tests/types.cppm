export module jinja.tests.types;

import reflex.jinja;
import std;

export namespace testing
{
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
//     extern template class var<bool, int, double, std::string, jinja::expr::loop_info&,
//     aggregate1&>; extern template class var<bool, int, double, std::string,
//     jinja::expr::loop_info&, aggregate2&>; extern template class var<bool, int, double,
//     std::string, jinja::expr::loop_info&, aggregate3&>;
// }
} // namespace testing

export namespace reflex::jinja
{
// extern template class expr::context<>;
extern template class expr::context<testing::aggregate1&>;
extern template class expr::context<testing::aggregate2&>;
extern template class expr::context<testing::aggregate3&>;
extern template class expr::context<testing::aggregate4&>;
extern template class expr::context<testing::aggregate5&>;
} // namespace reflex::jinja