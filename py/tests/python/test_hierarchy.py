import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))

import hierarchy  # noqa: E402
from harness import run  # noqa: E402


def test_a_base_is_published_with_its_derived_class():
    assert hierarchy.shape is not None
    assert issubclass(hierarchy.square, hierarchy.shape)
    assert isinstance(hierarchy.square(), hierarchy.shape)


def test_a_base_method_is_reachable_on_the_derived_class():
    assert hierarchy.square().name() == 1


def test_a_virtual_call_dispatches():
    assert hierarchy.square().area() == 4
    assert hierarchy.circle().area() == 3
    assert hierarchy.shape().area() == 0


def test_the_shared_base_is_registered_once():
    assert hierarchy.circle.__bases__ == (hierarchy.shape,)
    assert hierarchy.square.__bases__ == (hierarchy.shape,)


def test_binding_a_base_twice_is_a_no_op():
    # The module body binds shape explicitly after square already published it.
    assert hierarchy.square.__bases__[0] is hierarchy.shape


def test_a_skipped_base_is_not_published():
    assert not hasattr(hierarchy, "tag")
    assert hierarchy.untagged().value() == 4
    assert not hasattr(hierarchy.untagged(), "marker")


def test_a_private_base_is_not_published():
    assert not hasattr(hierarchy, "hidden_base")
    assert hierarchy.derived_privately().value() == 5
    assert not hasattr(hierarchy.derived_privately(), "leaked")


def test_a_nested_enum_lives_on_the_class():
    assert hierarchy.host.colour.Red is not None
    assert hierarchy.host.colour.Green.value == 1
    assert hierarchy.host.colour.Green == hierarchy.host.colour.Green
    assert hierarchy.host.colour.Green != hierarchy.host.colour.Red


def test_a_nested_enum_follows_the_naming_policy():
    assert hierarchy.host.colour.LightBlue is not None
    assert not hasattr(hierarchy.host.colour, "light_blue")


def test_a_nested_enum_carries_its_docstring():
    assert hierarchy.host.state.__doc__ == "a documented enumeration"


def test_an_enum_round_trips_through_a_method():
    assert hierarchy.host().tint() == hierarchy.host.colour.Green


def test_a_nested_class_lives_on_the_class():
    inner = hierarchy.host.inner()
    assert inner.value() == 6
    assert inner.held == 7
    assert hierarchy.host().make().value() == 6


def test_a_skipped_nested_class_is_absent():
    assert not hasattr(hierarchy.host, "secret")


def test_a_standalone_enum_binds_at_module_scope():
    # nanobind builds a real enum.Enum, so the underlying value is .value and
    # int() is not available without nb::is_arithmetic.
    assert hierarchy.standalone.two.value == 1


run(globals())
