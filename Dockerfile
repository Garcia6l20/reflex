# CI image for reflex: Ubuntu 26.04 + GCC 16.1.0 from Debian sid + Qt 6 (distro).
#
# Mirrors .github/workflows/main.yaml, plus the Qt packages reflex.qt needs.
# The Qt version is whatever Ubuntu ships, 6.10.2 today. reflex.qt static_asserts
# on 6.10.x and 6.11.x, both measured; another version fails the build on purpose,
# and `pcons REFLEX_QT_ALLOW_UNTESTED_QT=true` is the way past it.
#
#   docker build -t reflex-ci .
#   docker run --rm -v "$PWD:/work" -w /work reflex-ci bash -c '
#       uv run python -m pcons.packages.fetch.cli fetch deps.toml \
#           --deps-dir build/deps --output-dir build/pkg
#       uv run pcons generate REFLEX_BUILD_TESTS=ON
#       uv run pcons build && uv run pcons test'

FROM ubuntu:26.04

ARG DEBIAN_FRONTEND=noninteractive

SHELL ["/bin/bash", "-euo", "pipefail", "-c"]

# Ubuntu 26.04 ships GCC 16.0.1, whose reflection support miscompiles this code.
# GCC 16.1.0 fixes it but Ubuntu has no newer package, so pull the gcc-16 family
# from Debian sid. Pin priority 990 keeps only those packages (and their runtime
# libs) from Debian.
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
        ca-certificates curl git pkg-config ninja-build \
 && apt-get install -y --no-install-recommends debian-archive-keyring \
 && echo "deb [signed-by=/usr/share/keyrings/debian-archive-keyring.gpg] http://deb.debian.org/debian sid main" \
        > /etc/apt/sources.list.d/debian-sid.list \
 && printf 'Package: *\nPin: release o=Debian\nPin-Priority: 100\n\nPackage: gcc-16 gcc-16-* g++-16 g++-16-* cpp-16 cpp-16-* gcc-16-base libgcc-16-dev libstdc++-16-dev libstdc++6 libgcc-s1 libgomp1 libitm1 libatomic1 libquadmath0 libcc1-0 libasan8 libubsan1 liblsan0 libtsan2 libhwasan0\nPin: release o=Debian\nPin-Priority: 990\n' \
        > /etc/apt/preferences.d/debian \
 && apt-get update \
 && apt-get install -y --no-install-recommends gcc-16 g++-16 \
 && update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-16 100 \
 && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-16 100 \
 && gcc --version \
 && rm -rf /var/lib/apt/lists/*

# reflex.py builds a CPython extension, so the interpreter's headers have to be
# there. Without them the failure is a Python.h not found from inside nanobind.
RUN apt-get update \
 && apt-get install -y --no-install-recommends python3-dev \
 && rm -rf /var/lib/apt/lists/*

# reflex.qt needs Qt 6 Core, Widgets and Qml, plus moc and qmltyperegistrar as
# real binaries: the moc oracle and the cross-check test run them. The -dev
# packages pull the runtime QML plugins the engine test loads, and mesa/xkb are
# what Qt6Widgets links against.
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
        qt6-base-dev \
        qt6-base-dev-tools \
        qt6-declarative-dev \
        qt6-declarative-dev-tools \
        libgl1-mesa-dev \
        libxkbcommon-dev \
 && rm -rf /var/lib/apt/lists/* \
 && qmake6 -query QT_VERSION

COPY --from=ghcr.io/astral-sh/uv:latest /uv /uvx /usr/local/bin/

# pcons drives Conan itself at configure time, so the profile has to know about
# GCC 16 before the first `pcons generate`.
RUN mkdir -p /root/.conan2 \
 && printf 'compiler:\n  gcc:\n    version: ["16"]\n' > /root/.conan2/settings_user.yml

ENV CC=gcc-16 \
    CXX=g++-16 \
    UV_LINK_MODE=copy \
    UV_PROJECT_ENVIRONMENT=/opt/venv \
    PATH=/opt/venv/bin:$PATH \
    QT_QPA_PLATFORM=offscreen

# Resolve pcons and conan into a venv outside the mount point, so a bind-mounted
# source tree does not shadow them.
WORKDIR /work
COPY pyproject.toml ./
RUN uv sync \
 && conan profile detect --force \
 && rm -f pyproject.toml uv.lock

CMD ["/bin/bash"]
