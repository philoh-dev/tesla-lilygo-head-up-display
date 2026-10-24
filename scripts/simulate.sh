#!/usr/bin/env bash

set -euo pipefail

project_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        exec powershell.exe -NoProfile -ExecutionPolicy Bypass \
            -File "$(cygpath -w "${project_root}/scripts/simulate.ps1")" "$@"
        ;;
esac
build_directory="${project_root}/build-simulator"
dashboard_layout="${TESLA_HEADUP_DASHBOARD_LAYOUT:-true_hud}"

cmake \
    -S "${project_root}" \
    -B "${build_directory}" \
    -DTESLA_HEADUP_BUILD_SIMULATOR=ON \
    -DTESLA_HEADUP_BUILD_TESTS=OFF \
    -DTESLA_HEADUP_DASHBOARD_LAYOUT="${dashboard_layout}"
cmake --build "${build_directory}" --target tesla_headup_simulator --parallel
exec "${build_directory}/simulator/tesla_headup_simulator" "$@"
