#include <reflex/meta.hpp>

using namespace reflex;

void a_function();
void a_function_template(auto);
auto a_lambda                        = [] {};
auto an_int_lambda                   = [](int) {};
auto an_auto_lambda                  = [](auto) {};
auto an_variadic_lambda              = [](auto...) {};
auto a_ttp_template_lambda           = []<typename T> {};
auto a_nttp_template_lambda          = []<size_t I> {};
auto an_auto_nttp_template_lambda    = []<auto I> { return std::get<I>(std::tuple{1, 2, 3, 4, 5}); };
auto a_ttp_variadic_template_lambda  = []<typename... Ts>(auto...) {};
auto a_nttp_variadic_template_lambda = []<size_t... Is>(auto...) {};
struct a_non_callable_type
{
};
struct a_callable_type
{
  void operator()()
  {
  }
};

template <typename T>
struct a_template_callable_type
{
  void operator()(T&&)
  {
  }
};

template <size_t I>
struct an_nttp_template_callable_type
{
  void operator()(std::array<int, I> const&)
  {
  }
};

template <size_t I, typename T>
struct mixed1_template_callable_type
{
  void operator()(std::array<T, I> const&)
  {
  }
};


template <size_t I, typename T, typename TT>
struct mixed2_template_callable_type
{
  void operator()()
  {
  }
};

template <size_t I, typename T, size_t II, typename TT, typename TTT, size_t III>
struct mixed3_template_callable_type
{
  void operator()(std::array<T, I> const&, std::array<TT, II> const&, std::array<TTT, III> const&)
  {
  }
};

static_assert(not meta::has_call_operator(^^a_function));
static_assert(not meta::has_call_operator(^^a_function_template));
static_assert(meta::has_call_operator(^^a_lambda));
static_assert(meta::has_call_operator(^^an_int_lambda));
static_assert(meta::has_call_operator(^^an_auto_lambda));
static_assert(meta::has_call_operator(^^an_variadic_lambda));
static_assert(meta::has_call_operator(^^a_ttp_template_lambda));
static_assert(meta::has_call_operator(^^a_nttp_template_lambda));
static_assert(meta::has_call_operator(^^an_auto_nttp_template_lambda));
static_assert(meta::has_call_operator(^^a_ttp_variadic_template_lambda));
static_assert(meta::has_call_operator(^^a_nttp_variadic_template_lambda));
static_assert(meta::has_call_operator(^^a_callable_type));
static_assert(meta::has_call_operator(^^a_template_callable_type));
static_assert(meta::has_call_operator(^^an_nttp_template_callable_type));
static_assert(meta::has_call_operator(^^mixed1_template_callable_type));
static_assert(meta::has_call_operator(^^mixed2_template_callable_type));
static_assert(meta::has_call_operator(^^mixed3_template_callable_type));
static_assert(not meta::has_call_operator(^^a_non_callable_type));
static_assert(not meta::has_call_operator(^^std::string));

static_assert(is_function(^^a_function));
static_assert(not is_function(^^a_function_template));
static_assert(not is_function(^^a_lambda));

static_assert(meta::is_functional(^^a_function));
static_assert(meta::is_functional(^^a_function_template));
static_assert(meta::is_functional(^^a_lambda));
static_assert(meta::is_functional(^^an_int_lambda));
static_assert(meta::is_functional(^^an_auto_lambda));
static_assert(meta::is_functional(^^an_variadic_lambda));
static_assert(meta::is_functional(^^a_ttp_template_lambda));
static_assert(meta::is_functional(^^a_nttp_template_lambda));
static_assert(meta::is_functional(^^an_auto_nttp_template_lambda));
static_assert(meta::is_functional(^^a_ttp_variadic_template_lambda));
static_assert(meta::is_functional(^^a_nttp_variadic_template_lambda));
static_assert(meta::is_functional(^^a_callable_type));
static_assert(meta::is_functional(^^a_template_callable_type));
static_assert(meta::is_functional(^^an_nttp_template_callable_type));
static_assert(meta::is_functional(^^mixed1_template_callable_type));
static_assert(meta::is_functional(^^mixed2_template_callable_type));
static_assert(meta::is_functional(^^mixed3_template_callable_type));

static_assert(not meta::is_functional(^^a_non_callable_type));
static_assert(not meta::is_functional(^^int));
static_assert(not meta::is_functional(^^std::string));

#include <reflex/testing_main.hpp>

#define dump(...) std::println("{}: {}", #__VA_ARGS__, __VA_ARGS__)

template <meta::info R> void dump_lambda()
{
  std::println("+++++ {} +++++", display_string_of(R));
  constexpr auto type = is_type(R) ? R : type_of(R);
  constexpr auto members =
      define_static_array(members_of(type, meta::access_context::unchecked()) |
                          std::views::filter(
                              [](auto M)
                              {
                                return not is_constructor(M) and not is_special_member_function(M) and
                                       not has_identifier(M) and
                                       ((is_function(M) /* and not is_operator_function(M) */) or
                                        (is_function_template(M) and not is_conversion_function_template(M)));
                              }));
  template for(constexpr auto M : members)
  {
    std::println(" - {}", display_string_of(M));

    // dump(has_identifier(M));
    // dump(is_user_declared(M));
    // dump(is_function(M));
    // dump(is_function_template(M));
    // dump(is_operator_function(M));
    // dump(is_operator_function_template(M));
    // dump(is_conversion_function(M));
    // dump(is_conversion_function_template(M));

    if constexpr(is_function_template(M))
    {
      std::println(" => <function-template>");

      // dump(meta::is_functional(M));
      template for(constexpr auto n_args : std::views::iota(0uz, 16uz))
      {
        if constexpr(can_substitute(M, std::views::repeat(^^meta::any_arg, n_args)))
        {
          std::println(" == match ==");
          break;
        }
      }
    }
    else
    {
      dump(is_conversion_function(M));
      if constexpr(is_conversion_function(M))
      {
        constexpr auto ret = return_type_of(M);
        template for(constexpr auto n_args : std::views::iota(0uz, 16uz))
        {
          if constexpr(is_invocable_type(ret, std::views::repeat(^^meta::any_arg, n_args)))
          {
            std::println(" == match ==");
            break;
          }
        }
        // if constexpr(is_function(ret))
        // {
        //   std::println(" == match ==");
        // }
      }
      // std::println(" => {}", display_string_of(type_of(M)));
    }
  }
  std::println("----- {} -----", display_string_of(R));
}

namespace meta_tests
{
void test1()
{
  dump_lambda<^^a_lambda>();
  dump_lambda<^^an_int_lambda>();
  dump_lambda<^^an_auto_lambda>();
  dump_lambda<^^an_variadic_lambda>();
  dump_lambda<^^a_ttp_template_lambda>();
  dump_lambda<^^std::string>();
}
} // namespace meta_tests