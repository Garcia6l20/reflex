import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))

import operators  # noqa: E402
from harness import run  # noqa: E402


def n(value):
    return operators.number(value)


def test_binary_arithmetic():
    assert (n(6) + n(2)).value == 8
    assert (n(6) - n(2)).value == 4
    assert (n(6) * n(2)).value == 12
    assert (n(6) / n(2)).value == 3
    assert (n(7) % n(2)).value == 1


def test_unary_minus_is_told_from_binary_by_arity():
    assert (-n(6)).value == -6
    assert (n(6) - n(2)).value == 4


def test_bitwise():
    assert (~n(0)).value == -1
    assert (n(6) & n(3)).value == 2
    assert (n(1) << 3).value == 8


def test_in_place_addition_mutates_the_same_object():
    a = n(1)
    before = id(a)
    a += n(2)
    assert a.value == 3
    assert id(a) == before


def test_call():
    assert n(3)(4) == 12


def test_bool_follows_the_explicit_conversion():
    assert bool(n(1))
    assert not bool(n(0))


def test_a_non_explicit_conversion_is_not_bound():
    # operator int() is not explicit, so the object is not silently a number.
    try:
        n(3) + 1
    except TypeError:
        pass
    else:
        raise AssertionError("expected a TypeError")


def test_str_follows_the_explicit_conversion():
    value = operators.named()
    value.text = "written"
    assert str(value) == "written"


def test_a_renamed_operator_reaches_a_dunder_the_table_declines():
    assert abs(n(-4)).value == 4


def test_an_unmapped_operator_is_absent():
    assert not [name for name in dir(operators.number) if "operator" in name]
    assert not hasattr(n(1), "__iadd__x")


def test_a_spaceship_supplies_all_six_comparisons():
    a, b = operators.ordered(1), operators.ordered(2)
    assert a < b
    assert a <= b
    assert b > a
    assert b >= a
    assert a != b
    assert a == operators.ordered(1)


def test_sorting_uses_the_spaceship():
    values = [operators.ordered(3), operators.ordered(1), operators.ordered(2)]
    assert [v.rank for v in sorted(values)] == [1, 2, 3]


def test_an_explicit_equality_wins_over_the_spaceship():
    # The spaceship compares rank alone, the equality compares both fields.
    a = operators.specific(1, 10)
    b = operators.specific(1, 20)
    assert not (a < b)
    assert not (a == b)
    assert a != b
    assert a == operators.specific(1, 10)


def test_a_mutable_subscript_is_both_getter_and_setter():
    t = operators.table()
    t[2] = 9
    assert t[2] == 9


def test_a_const_subscript_is_read_only():
    t = operators.read_only_table()
    assert t[1] == 2
    try:
        t[1] = 5
    except TypeError:
        pass
    else:
        raise AssertionError("expected a TypeError")


run(globals())
