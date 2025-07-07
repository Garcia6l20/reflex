# reflex.cli

Command line interface for c++26.

## Example

```cpp
struct calculator : cli::command
{
    static constexpr auto description = "CLI calculator";

    [[= option<"-h/--help", "Print this message and exit.", _help>]] //
        bool show_help = false;

    [[= option<"-v/--verbose", "Show more details.", _repeat>]] //
        int verbose = false;

    struct base_subcommand : cli::command
    {
        [[= option<"-h/--help", "Print this message and exit.", _help>]] //
            bool show_help = false;

        [[= argument<"Left hand side value.">]] //
            float lhs;

        [[= argument<"Right hand side value.">]] //
            float rhs;
    };

    [[= sub_command]] struct _add : base_subcommand
    {
        static constexpr auto description = "Adds `LHS` to `RHS`.";

        int operator()() const noexcept
        {
            std::println("{}", lhs + rhs);
            return 0;
        }

    } add;

    [[= sub_command]] struct _sub : base_subcommand
    {
        static constexpr auto description = "Substracts `RHS` from `LHS`.";

        int operator()() const noexcept
        {
            std::println("{}", lhs - rhs);
            return 0;
        }

    } sub;

    [[= sub_command]] struct _mult : base_subcommand
    {
        static constexpr auto description = "Multiplies `LHS` by `RHS`.";

        int operator()() const noexcept
        {
            std::println("{}", lhs * rhs);
            return 0;
        }

    } mult;

    [[= sub_command]] struct _div : base_subcommand
    {
        static constexpr auto description = "Divide `LHS` by `RHS`.";

        int operator()() const noexcept
        {
            std::println("{}", lhs / rhs);
            return 0;
        }

    } div;
};
```

> See [tests](tests) for more examples.
