import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))

import library
from harness import run


def expect_type_error(call):
    try:
        call()
    except TypeError:
        return
    raise AssertionError("expected a TypeError")


def test_a_class_in_the_namespace_is_published():
    p = library.point(2, 3)
    assert p.sum() == 5
    assert p.x == 2


def test_an_enum_in_the_namespace_is_published():
    assert library.axis.vertical.value == 1


def test_a_skipped_type_is_absent():
    assert not hasattr(library, "internal_state")


def test_a_free_function():
    assert library.make(1, 2).sum() == 3


def test_a_free_function_returning_a_type_bound_in_the_same_pass():
    assert library.describe(library.point(4, 5)) == "4,5"


def test_a_free_overload_set_with_a_defaulted_arity():
    p = library.point(2, 3)
    assert library.scale(p, 2).sum() == 10
    assert library.scale(p, 0.5).sum() == 2
    assert library.scale(p).sum() == 10


def test_a_free_function_names_every_parameter():
    # No implicit object, so the first parameter is keyword addressable too.
    assert library.make(a=1, b=2).sum() == 3
    assert library.reflect(self=4) == -4


def test_a_skipped_free_function_is_absent():
    assert not hasattr(library, "internal_only")


def test_a_read_only_value_is_published():
    assert library.version == 3
    assert library.revision == 7


def test_a_mutable_value_is_not_published():
    assert not hasattr(library, "mutable_counter")


def test_an_unannotated_nested_namespace_is_absent():
    assert not hasattr(library, "detail")


def test_an_annotated_nested_namespace_becomes_a_submodule():
    assert library.extras.extra() == 2
    assert library.extras.level == 9


def test_a_free_operator_is_not_reached():
    expect_type_error(lambda: library.point(1, 1) + library.point(2, 2))
    assert not hasattr(library, "operator+")


def test_a_free_function_rejects_a_bad_argument():
    expect_type_error(lambda: library.make("a", "b"))


run(globals())
