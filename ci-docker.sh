#!/usr/bin/env bash
# Build and test reflex in the CI image (see Dockerfile).
#
#   ./ci-docker.sh              # build image if missing, then build + test
#   ./ci-docker.sh --rebuild    # rebuild the image first
#   ./ci-docker.sh --shell      # drop into the container instead
#   ./ci-docker.sh -- pcons test -L qt      # run one command in the container
#
# The container writes to build-ci/ (PCONS_BUILD_DIR), so the host build/ tree
# and its host toolchain paths are left alone.
set -euo pipefail

image=${REFLEX_CI_IMAGE:-reflex-ci}
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
docker=${DOCKER:-docker}

rebuild=0
mode=test
declare -a command=()

while [[ $# -gt 0 ]]; do
    case $1 in
        --rebuild) rebuild=1; shift ;;
        --shell) mode=shell; shift ;;
        --) shift; mode=command; command=("$@"); break ;;
        *) echo "unknown argument: $1" >&2; exit 2 ;;
    esac
done

mkdir -p "$root/build-ci/home"

if [[ $rebuild -eq 1 ]] || ! "$docker" image inspect "$image" >/dev/null 2>&1; then
    "$docker" build -t "$image" "$root"
fi

declare -a run=(
    "$docker" run --rm
    -v "$root:/work" -w /work
    -e PCONS_BUILD_DIR=/work/build-ci
    -e QT_QPA_PLATFORM=offscreen
    -u "$(id -u):$(id -g)"
    -e HOME=/work/build-ci/home
)

case $mode in
    shell) exec "${run[@]}" -it "$image" bash ;;
    command) exec "${run[@]}" "$image" "${command[@]}" ;;
esac

exec "${run[@]}" "$image" bash -euo pipefail -c '
    mkdir -p "$HOME/.conan2"
    printf "compiler:\n  gcc:\n    version: [\"16\"]\n" > "$HOME/.conan2/settings_user.yml"
    conan profile detect --force >/dev/null

    qmake6 -query QT_VERSION
    gcc --version | head -1

    python -m pcons.packages.fetch.cli fetch deps.toml \
        --deps-dir build-ci/deps --output-dir build-ci/pkg
    pcons generate REFLEX_BUILD_TESTS=ON REFLEX_BUILD_PROGRAMS=ON
    pcons build
    pcons test
'
