#!/usr/bin/env bash
#
# Compiles and runs every .ct fixture under test/ct2qjs/ through cthuc and bcrun
#
# Usage: test/run.sh
# Override the binaries with env vars if they're not in one of the usual
# build directories: CTHUC=/path/to/cthuc BCRUN=/path/to/bcrun test/run.sh

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
TEST_DIR="$REPO_ROOT/test/ct2qjs"
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

CTHUC_BIN="$(find_bin cthuc "${CTHUC:-}")" || {
    echo "error: could not find a built 'cthuc' binary. Build it first, or point to it with CTHUC=/path/to/cthuc" >&2
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
ct_files=("$TEST_DIR"/*.ct)
shopt -u nullglob

if [[ ${#ct_files[@]} -eq 0 ]]; then
    echo "no .ct fixtures found under $TEST_DIR"
    exit 2
fi

for ct_file in "${ct_files[@]}"; do
    name="$(basename "$ct_file")"
    tmp_qbc="$(mktemp)"

    if ! compile_out="$("$CTHUC_BIN" "$ct_file" -p "$PRELUDE_DIR" -o "$tmp_qbc" 2>&1)"; then
        echo "FAIL  $name  (compile error)"
        echo "$compile_out" | sed 's/^/        /'
        fail=$((fail + 1))
        fail_names+=("$name")
        rm -f "$tmp_qbc"
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

    rm -f "$tmp_qbc"
done

echo
echo "$pass passed, $fail failed"

if [[ $fail -gt 0 ]]; then
    echo "failed: ${fail_names[*]}"
    exit 1
fi

exit 0
