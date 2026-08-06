import gc
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))

import ownership
from harness import run


def test_a_reference_getter_hands_back_a_view():
    e = ownership.engine()
    e.settings().level = 5
    assert e.settings().level == 5


def test_a_const_reference_getter_is_a_view_too():
    e = ownership.engine()
    e.settings().level = 6
    assert e.readonly_settings().level == 6


def test_a_value_getter_still_copies():
    e = ownership.engine()
    e.settings().level = 7
    copy = e.by_value()
    copy.level = 8
    assert e.settings().level == 7


def test_a_view_keeps_its_parent_alive():
    view = ownership.engine().settings()
    gc.collect()
    # The engine has no other reference. reference_internal keeps it alive
    # through the view, so reading through it is not a use after free.
    view.level = 9
    assert view.level == 9


def test_an_annotated_pointer_is_not_taken_over():
    e = ownership.engine()
    found = e.find()
    found.level = 3
    assert e.find().level == 3
    del found
    gc.collect()
    # Still readable: the pointer was borrowed, not owned, so nothing freed it.
    assert e.find().level == 3


def test_a_unique_ptr_return_is_freed_exactly_once():
    assert ownership.engine.alive() == 0
    e = ownership.engine()
    owned = e.fresh()
    assert ownership.engine.alive() == 1
    del owned
    gc.collect()
    assert ownership.engine.alive() == 0


run(globals())
