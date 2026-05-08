export module reflex.core;

export import std;

#define REFLEX_MODULE

#define REFLEX_EXPORT       export
#define REFLEX_BEGIN_EXPORT export {
#define REFLEX_END_EXPORT   }

#include <reflex/caseconv.hpp>
#include <reflex/concepts.hpp>
#include <reflex/const_assert.hpp>
#include <reflex/ctp/ctp.hpp>
#include <reflex/constant.hpp>
#include <reflex/diags.hpp>
#include <reflex/enum.hpp>
#include <reflex/exception.hpp>
#include <reflex/formatters.hpp>
#include <reflex/heapless/string.hpp>
#include <reflex/heapless/vector.hpp>
#include <reflex/match.hpp>
#include <reflex/meta.hpp>
#include <reflex/meta/reg.hpp>
#include <reflex/named_arg.hpp>
#include <reflex/parse.hpp>
#include <reflex/scope_guard.hpp>
#include <reflex/to_tuple.hpp>
#include <reflex/utils.hpp>
#include <reflex/views/cartesian_product.hpp>
#include <reflex/visit.hpp>
#include <reflex/derive.hpp>
#include <reflex/format.hpp>
#include <reflex/tag_invoke.hpp>
#include <reflex/debug.hpp>
#include <reflex/named_tuple.hpp>
#include <reflex/overload_set.hpp>
