import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))

import overloads  # noqa: E402
from harness import run  # noqa: E402


def expect_type_error(call):
    try:
        call()
    except TypeError:
        return
    raise AssertionError("expected a TypeError")


def test_both_bindable_overloads_dispatch():
    w = overloads.widget()
    assert w.method() == 1
    assert w.method(3) == 30


def test_the_incomplete_overload_is_absent():
    w = overloads.widget()
    expect_type_error(lambda: w.method(object()))
    assert "some_type" not in (w.method.__doc__ or "")


def test_a_defaulted_parameter_is_reachable_at_both_arities():
    w = overloads.widget()
    assert w.scaled(5) == 10
    assert w.scaled(5, 3) == 15
    expect_type_error(lambda: w.scaled(5, 3, 1))


def test_an_unbindable_default_kills_only_the_long_arity():
    w = overloads.widget()
    assert w.partly(6) == 6
    expect_type_error(lambda: w.partly(6, None))


def test_a_const_and_non_const_pair_binds_once():
    w = overloads.widget()
    # The const one is kept: it is callable on strictly more objects.
    assert w.once() == 4


def test_an_rvalue_qualified_overload_is_absent():
    assert not hasattr(overloads.widget(), "only_rvalue")


def test_an_lvalue_qualified_overload_binds():
    assert overloads.widget().by_lvalue() == 7


def test_a_deducing_this_member_binds():
    w = overloads.widget()
    w.value = 5
    assert w.at(2) == 7


def test_overload_order_follows_declaration_order():
    w = overloads.widget()
    # pick(double) is declared first, pick(int) second. An exact match still
    # wins, because nanobind makes a no-conversion pass before a converting one.
    assert w.pick(1) == 200
    assert w.pick(1.5) == 100


def test_a_static_overload_set():
    assert overloads.widget.make() == 8
    assert overloads.widget.make(3) == 3


def test_a_static_defaulted_parameter():
    assert overloads.widget.counted(4) == 5
    assert overloads.widget.counted(4, 2) == 6


def test_skip_removes_only_the_annotated_overload():
    w = overloads.widget()
    assert w.both() == 9
    expect_type_error(lambda: w.both(1))


def test_a_rename_on_one_overload_renames_the_set():
    w = overloads.widget()
    assert w.renamed() == 11
    assert w.renamed(3) == 3
    assert not hasattr(w, "original")


run(globals())
