import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))

import hello  # noqa: E402
from harness import run  # noqa: E402


def test_add():
    assert hello.add(2, 3) == 5


def test_add_rejects_a_string():
    try:
        hello.add("a", "b")
    except TypeError:
        pass
    else:
        raise AssertionError("expected a TypeError")


def test_module_name():
    assert hello.__name__ == "hello"


run(globals())
