import gc
import sys
import weakref
from collections.abc import Callable
from pathlib import Path
from typing import Any

sys.path.insert(0, str(Path(__file__).parent))

import cycles
from harness import run


class Box:
    owner: Any = None


def cycle_through(
    make: Callable[[], Any], attach: Callable[[Any, Box], None]
) -> weakref.ref[Box]:
    owner = make()
    box = Box()
    attach(owner, box)
    box.owner = owner
    return weakref.ref(box)


def set_held(owner: Any, box: Box) -> None:
    owner.held = box


def test_an_annotated_member_makes_the_type_a_gc_type() -> None:
    assert gc.is_tracked(cycles.tracked())
    assert gc.is_tracked(cycles.hooked())
    assert not gc.is_tracked(cycles.holder())


def test_traverse_reports_what_the_member_holds() -> None:
    owner = cycles.tracked()
    box = Box()
    owner.held = box
    assert any(r is box for r in gc.get_referents(owner))


def test_a_cycle_through_an_annotated_member_is_collected() -> None:
    before = cycles.tracked.alive()
    ref = cycle_through(cycles.tracked, set_held)
    gc.collect()
    assert ref() is None
    assert cycles.tracked.alive() == before


def test_a_cycle_through_a_plain_member_is_not_collected() -> None:
    before = cycles.holder.alive()
    ref = cycle_through(cycles.holder, set_held)
    gc.collect()
    leaked = ref()
    assert leaked is not None
    assert cycles.holder.alive() == before + 1
    leaked.owner.held = object()
    del leaked
    gc.collect()
    assert ref() is None
    assert cycles.holder.alive() == before


def test_a_cycle_between_two_native_objects_is_broken_by_clear() -> None:
    before = cycles.tracked.alive()
    a = cycles.tracked()
    b = cycles.tracked()
    a.held = b
    b.held = a
    del a, b
    gc.collect()
    assert cycles.tracked.alive() == before


def test_a_python_subclass_holding_itself_is_collected() -> None:
    class Model(cycles.tracked):
        pass

    model = Model()
    model.held = model
    ref = weakref.ref(model)
    del model
    gc.collect()
    assert ref() is None


def test_a_python_subclass_of_a_plain_type_holding_itself_is_not_collected() -> None:
    class Model(cycles.holder):
        pass

    model = Model()
    model.held = model
    ref = weakref.ref(model)
    del model
    gc.collect()
    leaked = ref()
    assert leaked is not None
    leaked.held = object()
    del leaked
    gc.collect()
    assert ref() is None


def test_a_base_member_is_traversed_from_a_derived_class() -> None:
    ref = cycle_through(cycles.derived_tracked, set_held)
    gc.collect()
    assert ref() is None


def test_a_derived_member_is_traversed() -> None:
    def set_other(owner: Any, box: Box) -> None:
        owner.other = box

    ref = cycle_through(cycles.derived_tracked, set_other)
    gc.collect()
    assert ref() is None


def test_private_containers_are_traversed() -> None:
    def push(owner: Any, box: Box) -> None:
        owner.push(box)

    def name(owner: Any, box: Box) -> None:
        owner.name("box", box)

    def set_maybe(owner: Any, box: Box) -> None:
        owner.set_maybe(box)

    for attach in (push, name, set_maybe):
        ref = cycle_through(cycles.containers, attach)
        gc.collect()
        assert ref() is None, attach.__name__


def test_a_traverse_function_reports_what_it_owns() -> None:
    def keep(owner: Any, box: Box) -> None:
        owner.keep(box)

    before = cycles.hooked.alive()
    ref = cycle_through(cycles.hooked, keep)
    gc.collect()
    assert ref() is None
    assert cycles.hooked.alive() == before


def test_a_clear_function_breaks_a_native_cycle() -> None:
    before = cycles.hooked.alive()
    cleared = cycles.hooked.cleared()
    a = cycles.hooked()
    b = cycles.hooked()
    a.keep(b)
    b.keep(a)
    del a, b
    gc.collect()
    assert cycles.hooked.alive() == before
    assert cycles.hooked.cleared() > cleared


def test_gc_hooks_are_not_methods() -> None:
    assert not hasattr(cycles.hooked, "report")
    assert not hasattr(cycles.hooked, "forget")
    assert cycles.hooked().size() == 0


def test_a_view_reports_nothing_its_owner_holds() -> None:
    owner = cycles.outer()
    box = Box()
    owner.part.held = box
    assert all(r is not box for r in gc.get_referents(owner.part))


def test_a_view_cannot_make_the_collector_clear_a_live_object() -> None:
    owner = cycles.outer()

    def close_over_view() -> None:
        view = owner.part
        view.held = lambda: view

    close_over_view()
    gc.collect()
    held = owner.part.held
    assert held() is not None
    owner.part.held = object()


def test_each_referent_of_a_nested_container_is_visited_once() -> None:
    owner = cycles.nested()
    box = Box()
    keyed = Box()
    owner.put("a", box)
    owner.put_empty("a")
    owner.put_empty("b")
    owner.key(keyed)
    referents = gc.get_referents(owner)
    assert sum(r is box for r in referents) == 1
    assert sum(r is keyed for r in referents) == 1
    assert len(referents) == 3


def test_a_cycle_through_a_nested_container_is_collected() -> None:
    def put(owner: Any, box: Box) -> None:
        owner.put("a", box)

    def key(owner: Any, box: Box) -> None:
        owner.key(box)

    for attach in (put, key):
        ref = cycle_through(cycles.nested, attach)
        gc.collect()
        assert ref() is None, attach.__name__


def test_a_cycle_through_an_object_subclass_member_is_collected() -> None:
    owner = cycles.listed()
    box = Box()
    owner.items = [box]
    box.owner = owner
    ref = weakref.ref(box)
    del owner, box
    gc.collect()
    assert ref() is None


run(globals())
