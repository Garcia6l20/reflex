module jinja.tests.types;

import reflex.jinja;
import std;

namespace reflex::jinja
{
// extern template class expr::context<>;
template class expr::context<testing::aggregate1&>;
template class expr::context<testing::aggregate2&>;
template class expr::context<testing::aggregate3&>;
template class expr::context<testing::aggregate4&>;
template class expr::context<testing::aggregate5&>;
} // namespace reflex::jinja
