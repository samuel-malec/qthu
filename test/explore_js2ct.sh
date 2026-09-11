#!/usr/bin/env bash
#
# Ad hoc exploration tool for the js2ct -> ct2qjs -> bcrun pipeline.
# Feed it one .js file; it prints the JS source, the generated Cthulhu IR
# (.ct), and the QuickJS bytecode dump + run result, all in one place --
# for manual inspection, not automated pass/fail (see run_js2ct.sh for that).
#
# Usage: test/explore_js2ct.sh file.js [--ast] [--hir] [--linir]
#   --ast     also show js2ct's AST dump
#   --hir     also show js2ct's HIR dump
#   --linir   also show js2ct's linear-IR dump
#
# Override the binaries with env vars if they're not in one of the usual
# build directories:
#   JS2CT=/path/to/js2ct CT2QJS=/path/to/ct2qjs BCRUN=/path/to/bcrun test/explore_js2ct.sh file.js

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
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

if [[ $# -lt 1 ]]; then
    echo "Usage: $0 file.js [--ast] [--hir] [--linir]" >&2
    exit 2
fi

js_file="$1"
shift

if [[ ! -f "$js_file" ]]; then
    echo "error: no such file '$js_file'" >&2
    exit 2
fi

emit_flags=()
for arg in "$@"; do
    case "$arg" in
        --ast)   emit_flags+=("--emit-ast") ;;
        --hir)   emit_flags+=("--emit-hir") ;;
        --linir) emit_flags+=("--emit-linir") ;;
        *)
            echo "error: unknown flag '$arg'" >&2
            exit 2
            ;;
    esac
done

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

tmp_ct="$(mktemp)"
tmp_qbc="$(mktemp)"
trap 'rm -f "$tmp_ct" "$tmp_qbc"' EXIT

hr() { printf '%s\n' "----------------------------------------"; }

echo "=== JS source: $js_file ==="
hr
cat "$js_file"
hr
echo

if [[ ${#emit_flags[@]} -gt 0 ]]; then
    echo "=== js2ct: requested IR dumps ==="
    hr
    if ! "$JS2CT_BIN" "$js_file" -o "$tmp_ct" "${emit_flags[@]}"; then
        echo "js2ct failed" >&2
        exit 1
    fi
    hr
    echo
fi

echo "=== js2ct output: Cthulhu IR ($tmp_ct) ==="
hr
if [[ ${#emit_flags[@]} -eq 0 ]]; then
    if ! js2ct_out="$("$JS2CT_BIN" "$js_file" -o "$tmp_ct" 2>&1)"; then
        echo "js2ct failed:"
        echo "$js2ct_out"
        exit 1
    fi
fi
cat "$tmp_ct"
hr
echo

echo "=== ct2qjs output: QuickJS bytecode ==="
hr
if ! ct2qjs_out="$("$CT2QJS_BIN" "$tmp_ct" -p "$PRELUDE_DIR" -o "$tmp_qbc" 2>&1)"; then
    echo "ct2qjs failed:"
    echo "$ct2qjs_out"
    exit 1
fi
echo "compiled ok -> $(stat -c%s "$tmp_qbc" 2>/dev/null || wc -c < "$tmp_qbc") bytes"
hr
echo

echo "=== bcrun: bytecode dump + run result ==="
hr
"$BCRUN_BIN" "$tmp_qbc"
bcrun_status=$?
hr

exit "$bcrun_status"
