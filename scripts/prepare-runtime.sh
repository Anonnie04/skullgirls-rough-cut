#!/usr/bin/env bash
# Link the locally installed Steam archives without copying gigabytes of assets.
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
game=${1:-"$HOME/.local/share/Steam/steamapps/common/Skullgirls"}
mkdir -p "$root/runtime"
found=0
for name in data01 data02; do
    source="$game/$name"
    [[ -d "$source" ]] || continue
    source=$(cd -- "$source" && pwd -P)
    target="$root/runtime/$name"
    if [[ -e "$target" || -L "$target" ]]; then
        if [[ $(readlink -f -- "$target") != "$source" ]]; then
            echo "Refusing to replace existing $target" >&2
            exit 1
        fi
    else
        ln -s -- "$source" "$target"
    fi
    printf '%s -> %s\n' "$target" "$source"
    found=1
done
if [[ $found == 0 ]]; then
    echo "No data01 or data02 directory found under $game" >&2
    exit 1
fi
if [[ ! -d "$game/data02" ]]; then
    echo "This Steam installation has data01 only; no data02 link was created."
fi
