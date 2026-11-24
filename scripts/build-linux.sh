#!/usr/bin/env bash
# Build the native preview, asset tools and tests without requiring CMake.
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"
mkdir -p build-linux runtime
cxx=${CXX:-g++}
flags=(-std=c++17 -O2 -Isrc)
pkg-config --exists sdl2 cairo gl || { echo 'Install SDL2, Cairo, OpenGL development files and pkg-config.' >&2; exit 1; }
for unit in gfs sprite_rle sprite_layer foit foit_script sprite_asset audio_data; do
    "$cxx" "${flags[@]}" -c "src/$unit.cpp" -o "build-linux/$unit.o"
done
objects=(build-linux/{gfs,sprite_rle,sprite_layer,foit,foit_script,sprite_asset,audio_data}.o)
for tool in gfs rle sprite; do
    source="src/${tool}_main.cpp"
    [[ $tool != gfs ]] || source=src/main.cpp
    "$cxx" "${flags[@]}" "$source" "${objects[@]}" -o "build-linux/skullgirls-$tool"
    cp "build-linux/skullgirls-$tool" runtime/
done
for test in foit_script runtime_data stage_scene hud_portrait stage_select audio_data; do
    "$cxx" "${flags[@]}" "tests/${test}_test.cpp" "${objects[@]}" -o "build-linux/$test-test"
    "build-linux/$test-test"
done
read -ra platform_cflags <<< "$(pkg-config --cflags sdl2 cairo gl)"
read -ra platform_libs <<< "$(pkg-config --libs sdl2 cairo gl)"
"$cxx" "${flags[@]}" -pthread "${platform_cflags[@]}" src/main_menu.cpp \
    src/audio.cpp src/menu_timings.cpp src/title_scene.cpp src/fighter_select.cpp src/select_idle.cpp \
    "${objects[@]}" "${platform_libs[@]}" -ldl -o build-linux/skullgirls-main-menu
cp build-linux/skullgirls-main-menu runtime/
echo "Linux asset tools built in runtime/; all six portable tests passed."
echo "Launch the native preview with: ./runtime/skullgirls-main-menu"
