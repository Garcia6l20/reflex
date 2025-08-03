#pragma once

#define __PP_PARENS ()

#define __PP_EXPAND(...)  __PP_EXPAND4(__PP_EXPAND4(__PP_EXPAND4(__PP_EXPAND4(__VA_ARGS__))))
#define __PP_EXPAND4(...) __PP_EXPAND3(__PP_EXPAND3(__PP_EXPAND3(__PP_EXPAND3(__VA_ARGS__))))
#define __PP_EXPAND3(...) __PP_EXPAND2(__PP_EXPAND2(__PP_EXPAND2(__PP_EXPAND2(__VA_ARGS__))))
#define __PP_EXPAND2(...) __PP_EXPAND1(__PP_EXPAND1(__PP_EXPAND1(__PP_EXPAND1(__VA_ARGS__))))
#define __PP_EXPAND1(...) __VA_ARGS__

#define PP_FOR_EACH(macro, ...)              __VA_OPT__(__PP_EXPAND(__PP_FOR_EACH_HELPER(macro, __VA_ARGS__)))
#define __PP_FOR_EACH_HELPER(macro, a1, ...) macro(a1) __VA_OPT__(__PP_FOR_EACH_AGAIN __PP_PARENS(macro, __VA_ARGS__))
#define __PP_FOR_EACH_AGAIN()                __PP_FOR_EACH_HELPER

#define PP_COMMA_JOIN(macro, ...) __VA_OPT__(__PP_EXPAND(__PP_COMMA_JOIN_HELPER(macro, __VA_ARGS__)))
#define __PP_COMMA_JOIN_HELPER(macro, a1, ...) \
  macro(a1) __VA_OPT__(, ) __VA_OPT__(__PP_COMMA_JOIN_AGAIN __PP_PARENS(macro, __VA_ARGS__))
#define __PP_COMMA_JOIN_AGAIN() __PP_COMMA_JOIN_HELPER

#define __PP_CONCAT(__l__, __r__) __l__##__r__
#define PP_CONCAT(__l__, __r__)   __PP_CONCAT(__l__, __r__)
