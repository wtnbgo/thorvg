#!/usr/bin/env bash
# Build thorvg twice (TVG_SIMD=OFF / ON) for aarch64 and verify
# tvg-raster-test output is bit-identical. Runs under qemu-user when not on
# an aarch64 host (e.g. WSL x86_64 with qemu-aarch64 + g++-aarch64-linux-gnu).
#
# Usage: tools/raster-test/compare-simd.sh [build-root]
#   CROSS=0        native build (default: cross to aarch64 unless uname -m is aarch64)
#   CXX_AARCH64=   cross compiler prefix override (default aarch64-linux-gnu-)
set -eu

src="$(cd "$(dirname "$0")/../.." && pwd)"
root="${1:-/tmp/tvg-raster-test}"

host_arch="$(uname -m)"
cross=${CROSS:-}
if [ -z "$cross" ]; then
    if [ "$host_arch" = "aarch64" ]; then cross=0; else cross=1; fi
fi

common=(
    -DCMAKE_BUILD_TYPE=Release
    -DTVG_BUILD_SHARED=OFF
    -DTVG_THREADS=OFF
    -DTVG_PARTIAL=OFF
    -DTVG_LOADER_SVG=OFF
    -DTVG_LOADER_LOTTIE=OFF
    -DTVG_LOADER_TTF=OFF
    -DTVG_LOTTIE_EXPRESSIONS=OFF
    -DTVG_TOOL_RASTER_TEST=ON
    -DCMAKE_DISABLE_FIND_PACKAGE_OpenMP=TRUE
)
runner=()
if [ "$cross" = "1" ]; then
    prefix="${CXX_AARCH64:-aarch64-linux-gnu-}"
    common+=(
        -DCMAKE_SYSTEM_NAME=Linux
        -DCMAKE_SYSTEM_PROCESSOR=aarch64
        "-DCMAKE_C_COMPILER=${prefix}gcc"
        "-DCMAKE_CXX_COMPILER=${prefix}g++"
        -DCMAKE_EXE_LINKER_FLAGS=-static
    )
    runner=(qemu-aarch64)
fi

gen=()
command -v ninja >/dev/null 2>&1 && gen=(-G Ninja)

declare -A results
for simd in OFF ON; do
    bdir="$root/simd-$simd"
    echo "=== configure+build TVG_SIMD=$simd -> $bdir"
    cmake -S "$src" -B "$bdir" "${gen[@]}" "${common[@]}" "-DTVG_SIMD=$simd" >/dev/null
    cmake --build "$bdir" --target tvg-raster-test >/dev/null
    out="$("${runner[@]}" "$bdir/tvg-raster-test")"
    results[$simd]="$out"
    echo "$out"
done

echo "=== diff"
if [ "${results[OFF]}" = "${results[ON]}" ]; then
    echo "PASS: SIMD ON/OFF outputs are bit-identical"
else
    diff <(echo "${results[OFF]}") <(echo "${results[ON]}") || true
    echo "FAIL: outputs differ"
    exit 1
fi
