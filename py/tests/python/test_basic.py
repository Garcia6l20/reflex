import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))

import basic  # noqa: E402
from harness import run  # noqa: E402


def expect_type_error(call):
    try:
        call()
    except TypeError:
        return
    raise AssertionError("expected a TypeError")


def test_constructs_from_the_bound_signature():
    assert basic.my_class(2, 3).sum() == 5


def test_the_incomplete_parameter_overload_is_absent():
    expect_type_error(lambda: basic.my_class(object()))


def test_the_skipped_constructor_is_absent():
    expect_type_error(lambda: basic.my_class("abc"))


def test_no_default_constructor_means_no_nullary_call():
    expect_type_error(basic.my_class)


def test_an_empty_class_is_default_constructible():
    assert basic.empty() is not None


def test_a_deleted_constructor_is_absent():
    assert basic.only_default() is not None
    expect_type_error(lambda: basic.only_default(1))


def test_a_private_constructor_is_absent():
    expect_type_error(basic.hidden_ctor)


def test_the_type_name_follows_the_rename():
    assert basic.renamed() is not None
    assert not hasattr(basic, "awkward_name")


def test_an_explicit_name_wins():
    assert basic.given() is not None
    assert not hasattr(basic, "explicitly_named")


def test_copy_is_not_exposed():
    # Pinned rather than assumed. Dropping the copy constructor from the
    # __init__ set leaves nothing for copy.copy to reach, and it falls through
    # to the pickle protocol, which is not bound either.
    import copy

    expect_type_error(lambda: copy.copy(basic.my_class(2, 3)))


run(globals())
