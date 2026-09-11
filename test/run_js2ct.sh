#!/usr/bin/env bash
#
# Compiles and runs every .js fixture under test/js2ct/e2e/ through the
# complete pipeline: js2ct (JS -> Cthulhu IR) -> ct2qjs (IR -> QuickJS
# bytecode) -> bcrun. Each fixture self-verifies with assert(); a failing
# assert (or a compile error at either stage) fails the fixture.
#
# Usage: test/run_js2ct.sh
# Override the binaries with env vars if they're not in one of the usual
# build directories:
#   JS2CT=/path/to/js2ct CT2QJS=/path/to/ct2qjs BCRUN=/path/to/bcrun test/run_js2ct.sh

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
TEST_DIR="$REPO_ROOT/test/js2ct/e2e"
PRELUDE_DIR="$REPO_ROOT/src/cthu_core/js_dial"

find_bin()
{
    local name="$1"
    local override="$2"

    if [[ -n "$override" ]]; then
        if [[ -x "$override" ]]; then
            echo "$override"
            return 0
        fi
        echo "error: $name override '$override' is not an executable file" >&2
        return 1
    fi

    local candidate
    for candidate in \
        "$REPO_ROOT/cmake-build-debug/$name" \
        "$REPO_ROOT/out/build/qthu/$name" \
        "$REPO_ROOT/build/$name"
    do
        if [[ -x "$candidate" ]]; then
            echo "$candidate"
            return 0
        fi
    done

    return 1
}

JS2CT_BIN="$(find_bin js2ct "${JS2CT:-}")" || {
    echo "error: could not find a built 'js2ct' binary. Build it first, or point to it with JS2CT=/path/to/js2ct" >&2
    exit 2
}
CT2QJS_BIN="$(find_bin ct2qjs "${CT2QJS:-}")" || {
    echo "error: could not find a built 'ct2qjs' binary. Build it first, or point to it with CT2QJS=/path/to/ct2qjs" >&2
    exit 2
}
BCRUN_BIN="$(find_bin bcrun "${BCRUN:-}")" || {
    echo "error: could not find a built 'bcrun' binary. Build it first, or point to it with BCRUN=/path/to/bcrun" >&2
    exit 2
}

pass=0
fail=0
fail_names=()

shopt -s nullglob
js_files=("$TEST_DIR"/*.js)
shopt -u nullglob

if [[ ${#js_files[@]} -eq 0 ]]; then
    echo "no .js fixtures found under $TEST_DIR"
    exit 2
fi

for js_file in "${js_files[@]}"; do
    name="$(basename "$js_file")"
    tmp_ct="$(mktemp)"
    tmp_qbc="$(mktemp)"

    if ! lower_out="$("$JS2CT_BIN" "$js_file" -o "$tmp_ct" 2>&1)"; then
        echo "FAIL  $name  (js2ct: JS -> Cthulhu IR failed)"
        echo "$lower_out" | sed 's/^/        /'
        fail=$((fail + 1))
        fail_names+=("$name")
        rm -f "$tmp_ct" "$tmp_qbc"
        continue
    fi

    if ! compile_out="$("$CT2QJS_BIN" "$tmp_ct" -p "$PRELUDE_DIR" -o "$tmp_qbc" 2>&1)"; then
        echo "FAIL  $name  (ct2qjs: Cthulhu IR -> bytecode failed)"
        echo "$compile_out" | sed 's/^/        /'
        fail=$((fail + 1))
        fail_names+=("$name")
        rm -f "$tmp_ct" "$tmp_qbc"
        continue
    fi

    if run_out="$("$BCRUN_BIN" "$tmp_qbc" 2>&1)"; then
        echo "PASS  $name"
        pass=$((pass + 1))
    else
        echo "FAIL  $name  (assert failed at runtime)"
        fail=$((fail + 1))
        fail_names+=("$name")
    fi

    rm -f "$tmp_ct" "$tmp_qbc"
done

echo
echo "$pass passed, $fail failed"

if [[ $fail -gt 0 ]]; then
    echo "failed: ${fail_names[*]}"
    exit 1
fi

exit 0
