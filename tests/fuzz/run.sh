#!/bin/sh
# Build and run the libFuzzer harnesses under ASan/UBSan. Needs clang with libFuzzer, Autotools
# and python3; meant for the disposable test container. FUZZ_SECONDS bounds each target.
set -eu
src=$(CDPATH='' cd -- "$(dirname "$0")/../.." && pwd)
seconds=${FUZZ_SECONDS:-10}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
cp -R "$src" "$work/tree"
cd "$work/tree"
./autogen.sh >/dev/null 2>&1
./configure >/dev/null 2>&1
flags="-g -O1 -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=undefined -I. -Isrc"
defs='-DSOFTGRED_CONFDIR="/etc/softgred" -DSOFTGRED_PIDDIR="/run"'
python3 tests/fuzz/seed.py "$work/corpus-parser" "$work/corpus-config"
run() {
    name=$1
    shift
    echo "=== fuzz $name (${seconds}s) ==="
    # shellcheck disable=SC2086
    clang $flags $defs "$@" -o "$work/$name"
    mkdir -p "$work/corpus-$name"
    limits="-max_total_time=$seconds -max_len=512"
    # shellcheck disable=SC2086
    if ! "$work/$name" "$work/corpus-$name" $limits >"$work/$name.log" 2>&1; then
        tail -40 "$work/$name.log"
        echo "fuzz $name FAILED" >&2
        exit 1
    fi
    grep -E "^Done" "$work/$name.log"
}
mkdir -p "$work/corpus-parser"
run parser src/payload.c tests/fuzz/parser_fuzz.c
run hashmap src/hashmap.c src/compat.c tests/fuzz/hashmap_fuzz.c
run config src/softgred_config.c src/compat.c tests/fuzz/config_fuzz.c
echo 'Fuzzing: parser, hashmap and config loader found no crashes, leaks or model divergences'
