#!/bin/sh
# Tests host des modules firmware sans dépendance matérielle (gcc + ASan/UBSan).
#   firmware/test_host/run.sh
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
FW=$(cd "$HERE/.." && pwd)
SC="$FW/components/scenario"
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT

CFLAGS="-std=c11 -g -fsanitize=address,undefined -fno-sanitize-recover=all"
gcc $CFLAGS -I"$HERE/stub" -I"$SC" -c "$SC/cJSON.c" -o "$OUT/cJSON.o"
gcc $CFLAGS -Wall -Wextra -Werror -DFW_DIR="\"$FW\"" \
    -I"$HERE/stub" -I"$SC" -I"$SC/include" \
    "$HERE/test_scenario_validate.c" "$SC/scenario_validate.c" "$OUT/cJSON.o" \
    -o "$OUT/test_scenario_validate"
"$OUT/test_scenario_validate" 2>/dev/null
