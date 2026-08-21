import os
from pathlib import Path

from pcons import Configure, find_c_toolchain, get_var, get_variant

# =============================================================================
# Configuration
# =============================================================================

VARIANT = get_variant("release")

project_dir = Path(__file__).parent.parent
build_dir = Path(os.environ.get("PCONS_BUILD_DIR", project_dir / "build"))

config = Configure(build_dir=build_dir)
toolchain = find_c_toolchain(prefer=["gcc"])

coverage = get_var("REFLEX_COVERAGE", False)
build_testing = get_var("REFLEX_BUILD_TESTS", False) or coverage
build_programs = get_var("REFLEX_BUILD_PROGRAMS", False)
