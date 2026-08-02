import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))

import args  # noqa: E402
from harness import run  # noqa: E402


def expect_type_error(call):
    try:
        call()
    except TypeError:
        return
    raise AssertionError("expected a TypeError")


def test_keyword_arguments_at_both_arities():
    w = args.widget()
    assert w.scaled(n=5) == 10
    assert w.scaled(n=5, k=3) == 15
    assert w.scaled(5, k=3) == 15


def test_an_unknown_keyword_is_rejected():
    expect_type_error(lambda: args.widget().scaled(bad=1))


def test_a_partly_named_signature_falls_back_to_positional():
    w = args.widget()
    assert w.partly_named(1, 2) == 1
    expect_type_error(lambda: w.partly_named(named=1, arg1=2))


def test_a_zero_parameter_method_still_works():
    assert args.widget().nothing() == 1


def test_the_object_is_not_an_argument():
    w = args.widget()
    w.count = 5
    assert w.at(2) == 7
    assert w.at(n=2) == 7


def test_a_static_method_names_every_parameter():
    assert args.widget.twice(value=3) == 6


def test_a_parameter_named_self_is_passed_through():
    # `self` and `lambda` are ordinary identifiers in C++ and reach Python
    # unmangled. Only a keyword call is affected, and `self` there collides with
    # the bound method's own first argument.
    w = args.widget()
    assert w.shadowing(1, 2) == 3
    assert w.shadowing(1, **{"lambda": 2}) == 3
    # nanobind names its own first argument `self` too, so the signature carries
    # it twice and a self= keyword call is not reachable.
    assert "shadowing(self, self: int, lambda: int)" in args.widget.shadowing.__doc__


def test_the_class_docstring():
    assert args.widget.__doc__ == "a documented type"


def test_a_member_docstring():
    assert args.widget.count.__doc__ == "how many times it turned"


def test_a_method_docstring():
    assert args.widget.documented.__doc__.endswith('turns\nand "turns"')


def test_an_undocumented_method_keeps_its_signature():
    doc = args.widget.undocumented.__doc__
    assert doc is not None and "(" in doc


def test_an_undocumented_class_has_no_empty_docstring():
    assert not args.plain.__doc__


run(globals())
