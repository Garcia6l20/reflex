import os
from pathlib import Path

from pcons import get_variant, find_c_toolchain, Configure

# =============================================================================
# Configuration
# =============================================================================

VARIANT = get_variant("release")

project_dir = Path(os.environ.get("PCONS_SOURCE_DIR", Path(__file__).parent.parent))
build_dir = Path(os.environ.get("PCONS_BUILD_DIR", project_dir / "build"))

config = Configure(build_dir=build_dir)
toolchain = find_c_toolchain(prefer=["gcc"])

if not config.get("configured") or os.environ.get("PCONS_RECONFIGURE"):
    toolchain.configure(config)
    config.set("configured", True)
    config.save()