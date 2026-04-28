#include <doctest/doctest.h>

import reflex.cli;

using namespace reflex;


struct [[=cli::command{"Cli1"}]] cli1 {

    [[= cli::option{"-a/--all", "All the things!"}.flag()]] bool all{false};

    int operator()() const
    {
        return 0;
    }
};

TEST_CASE("reflex::cli: dev test")
{
    using namespace reflex::cli;
    using namespace reflex::cli::detail;

    static constexpr auto _raw = raw_parse<^^cli1>();
    
    [[maybe_unused]] static constexpr constant<std::vector<argument_info>> args =
        argument_info::from_info_range(std::get<0>(_raw)) | std::ranges::to<std::vector>();
    static constexpr constant<std::vector<option_info>> opts =
        option_info::from_info_range(std::get<1>(_raw)) | std::ranges::to<std::vector>();
    [[maybe_unused]] static constexpr constant<std::vector<command_info>> cmds =
        command_info::from_info_range(std::get<2>(_raw)) | std::ranges::to<std::vector>();

    static_assert(opts->size() == 2); // help added automatically, so we should have 2 options: help and all
    static_assert(opts->at(0) == ^^help_option);
    static_assert(opts->at(1) == ^^cli1::all);
    static_assert(opts->at(1).switches.s == "-a");
    static_assert(opts->at(1).switches.l == "--all");
    static_assert(opts->at(1).help() == "All the things!");
    static_assert(opts->at(1).is_flag() == true);
    static_assert(opts->at(1).is_counter() == false);

    item_tracker<opts> tracker{};

    std::println("Tracker value_type: {}", display_string_of(dealias(^^item_tracker<opts>::value_type)));

    CHECK_FALSE(tracker.is_used(0));

    cli1 c{};
    [[maybe_unused]] cli::detail::parse_trackers<cli1> trackers{c};
}
