#!/bin/bash
# run-dosbox.sh — launch DOSBox-Staging (NukedOPL by default) with the
# adlib-pcm build. Use this when QEMU's adlib device doesn't react to
# rapid TL register writes the way real hardware does.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
STAGE="$REPO_ROOT/build/dosbox"

if [[ ! -f "$REPO_ROOT/build/adlib.exe" ]]; then
    echo "ERROR: build/adlib.exe missing. Run 'make all raws' first."
    exit 1
fi

mkdir -p "$STAGE"
# DOSBox prefers 8.3 names; the EXE and .RAW files already follow that.
cp -f "$REPO_ROOT/build/adlib.exe" "$STAGE/ADLIB.EXE"
for f in "$REPO_ROOT"/assets/*.RAW; do
    [[ -f "$f" ]] || { echo "ERROR: no .RAW files in assets/. Run 'make raws'."; exit 1; }
    cp -f "$f" "$STAGE/$(basename "$f")"
done

exec dosbox-staging --conf "$REPO_ROOT/scripts/dosbox.conf"
