#!/usr/bin/env bash
set -euo pipefail
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
qmk_tree="${1:?Usage: tools/build.sh /path/to/qmk_firmware}"
python3 "$repo/tools/prepare_firmware.py" "$qmk_tree"
cd "$qmk_tree"
git submodule update --init --depth 1 lib/chibios lib/chibios-contrib lib/printf lib/lufa
qmk compile -kb designedbygg/redblade/rbfs -km aurora_via -e SKIP_GIT=yes -j "${JOBS:-4}"
mkdir -p "$repo/dist"
cp designedbygg_redblade_rbfs_aurora_via.bin "$repo/dist/redblade-fs-aurora-via.bin"
sha256sum "$repo/dist/redblade-fs-aurora-via.bin"

