#!/usr/bin/env bash
# Run the existing combat regression suites against the local Steam archives.
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"
data=${1:-"$root/runtime/data02"}
if [[ ! -f "$data/characters-foits.gfs" && -f "$root/runtime/data01/characters-foits.gfs" ]]; then
    data="$root/runtime/data01"
fi
[[ -f "$data/characters-foits.gfs" ]] || { echo 'Prepare the local game data first.' >&2; exit 1; }
cxx=${CXX:-g++}
read -ra platform_cflags <<< "$(pkg-config --cflags sdl2 cairo gl)"
read -ra platform_libs <<< "$(pkg-config --libs sdl2 cairo gl)"
objects=(build-linux/{gfs,sprite_rle,sprite_layer,foit,foit_script,sprite_asset}.o)
failed=0
for fighter in filia cerebella peacock parasoul msfortune painwheel intro reference; do
    echo "Building and testing $fighter"
    "$cxx" -std=c++17 -O1 -pthread -Isrc "${platform_cflags[@]}" "tests/${fighter}_moves_test.cpp" \
        src/menu_timings.cpp src/title_scene.cpp src/fighter_select.cpp src/select_idle.cpp \
        "${objects[@]}" "${platform_libs[@]}" -o "build-linux/$fighter-moves-test"
    if ! "build-linux/$fighter-moves-test" "$data"; then
        echo "FAILED: $fighter"
        failed=1
    fi
done
exit "$failed"
