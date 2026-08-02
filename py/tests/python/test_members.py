import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))

import members  # noqa: E402
from harness import run  # noqa: E402


def expect_raise(call, kind):
    try:
        call()
    except kind:
        return
    raise AssertionError(f"expected a {kind.__name__}")


def test_a_data_member_round_trips():
    w = members.widget()
    assert w.value == 0
    w.value = 5
    assert w.value == 5


def test_a_const_member_is_read_only():
    w = members.widget()
    assert w.fixed == 7
    expect_raise(lambda: setattr(w, "fixed", 1), AttributeError)


def test_an_annotated_member_is_read_only():
    w = members.widget()
    assert w.serial == 42
    expect_raise(lambda: setattr(w, "serial", 1), AttributeError)


def test_a_skipped_member_is_absent():
    assert not hasattr(members.widget(), "internal")


def test_a_static_data_member_lives_on_the_class():
    assert members.widget.made == 5
    members.widget.made = 6
    assert members.widget.made == 6
    members.widget.made = 5


def test_a_const_method():
    w = members.widget()
    w.value = 4
    assert w.doubled() == 8


def test_a_mutating_method():
    w = members.widget()
    assert w.bump(3) == 3
    assert w.value == 3


def test_a_static_method_needs_no_instance():
    assert members.widget.tag() == 99


def test_a_skipped_method_is_absent():
    assert not hasattr(members.widget(), "hidden")


def test_a_renamed_method():
    w = members.widget()
    assert w.renamed() == 2
    assert not hasattr(w, "original")


def test_a_member_template_is_absent():
    assert not hasattr(members.widget(), "generic")


def test_an_incomplete_signature_is_absent():
    w = members.widget()
    assert not hasattr(w, "opaque")
    assert not hasattr(w, "returns_opaque")


def test_no_attribute_is_named_after_an_operator():
    assert not [name for name in dir(members.widget) if name.startswith("operator")]


def test_a_private_member_is_absent():
    g = members.guarded()
    assert g.visible() == 3
    assert not hasattr(g, "secret_")
    assert not hasattr(g, "invisible")


def test_a_class_with_no_public_member_still_binds():
    assert members.bare() is not None


run(globals())
