import os
from pathlib import Path

from pcons import get_variant, get_var, find_c_toolchain, Configure

# =============================================================================
# Configuration
# =============================================================================

VARIANT = get_variant("release")

project_dir = Path(os.environ.get("PCONS_SOURCE_DIR", Path(__file__).parent.parent))
build_dir = Path(os.environ.get("PCONS_BUILD_DIR", project_dir / "build"))

config = Configure(build_dir=build_dir)
toolchain = find_c_toolchain(prefer=["gcc"])

test_enabled_in_config = bool(config.get("reflex_build_tests", False))
test_enabled_in_env = get_var("REFLEX_BUILD_TESTS")
if test_enabled_in_env is not None:
    test_enabled_in_env = test_enabled_in_env in ("ON", "1", "TRUE", "YES")


_all_options = list()

class Option:
    def __init__(self, name: str, default: bool = False):
        self.name = name
        self.enabled_in_config = bool(config.get(self.name, default))
        self.enabled_in_env = get_var(self.name)
        if self.enabled_in_env is not None:
            self.enabled_in_env = self.enabled_in_env in ("ON", "1", "TRUE", "YES")
        if self.needs_reconfigure:
            config.set(self.name, bool(self))
        _all_options.append(self)

    @property
    def needs_reconfigure(self) -> bool:
        if self.enabled_in_env is not None:
            return self.enabled_in_env != self.enabled_in_config
        return False

    def __bool__(self):
        if self.enabled_in_env is not None:
            return self.enabled_in_env
        return self.enabled_in_config


build_testing = Option("REFLEX_BUILD_TESTS", default=False)

should_reconfigure = (
    not config.get("configured")
    or os.environ.get("PCONS_RECONFIGURE")
    or any(option.needs_reconfigure for option in _all_options)
)
if should_reconfigure:
    toolchain.configure(config)
    config.set("configured", True)
    config.save()
