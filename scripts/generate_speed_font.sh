#!/usr/bin/env bash

set -euo pipefail

project_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"

generate_font() {
    local size="$1"

    npx --yes lv_font_conv@1.5.3 \
        --font assets/fonts/Montserrat-Light.ttf \
        --range 0x30-0x39 \
        --size "${size}" \
        --bpp 4 \
        --format lvgl \
        --no-compress \
        --no-prefilter \
        --no-kerning \
        --lv-include lvgl.h \
        --lv-font-name "tesla_headup_speed_font_${size}" \
        --output "components/adapters/lvgl_rm67162/assets/tesla_headup_speed_font_${size}.c"
}

cd "${project_root}"
generate_font 64
generate_font 110

for size in 110 40; do
    npx --yes lv_font_conv@1.5.3 \
        --font assets/fonts/Montserrat-Bold.ttf --range 0x30-0x39 \
        --size "${size}" --bpp 4 --format lvgl --no-compress --no-prefilter \
        --no-kerning --lv-include lvgl.h --lv-font-name "tesla_headup_bold_font_${size}" \
        --output "components/adapters/lvgl_rm67162/assets/tesla_headup_bold_font_${size}.c"
done
