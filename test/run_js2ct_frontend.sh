#!/usr/bin/env bash
#
# Tests js2ct's lexer and parser in isolation via --parse-only, which stops
# right after parsing (skips sema/HIR/LIN/codegen entirely). That matters:
# without it, a "good" fixture using syntax the parser accepts but a later
# stage doesn't yet support (e.g. plain `&`/`|`/`^`/`~`, or `&&`/`||`, whose
# short-circuit lowering is still unimplemented) would look like a parser
# failure when it isn't one.
#
# Covers:
#   test/js2ct/lexer/good/*.js   -- must parse successfully
#   test/js2ct/lexer/bad/*.js    -- must fail to lex/parse, cleanly
#   test/js2ct/parser/good/*.js  -- must parse successfully
#   test/js2ct/parser/bad/*.js   -- must fail to parse, cleanly
#
# "Cleanly" means a thrown exception (nonzero exit), not a crash (killed by
# a signal, e.g. SIGABRT/SIGSEGV) -- a malformed-input fixture that crashes
# the compiler is its own bug, distinct from "correctly rejected". Bash
# reports a signal-killed process as exit code 128+signum; js2ct's own
# caught-exception path always exits 255 (-1 as an unsigned byte), which
# doesn't collide with any real signal's 128+N range.
#
# Usage: test/run_js2ct_frontend.sh
# Override the binary with an env var if it's not in one of the usual
# build directories: JS2CT=/path/to/js2ct test/run_js2ct_frontend.sh

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

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

pass=0
fail=0
fail_names=()

# check_fixture <file> <expect: good|bad>
check_fixture()
{
    local file="$1"
    local expect="$2"
    local name
    name="$(basename "$file")"

    local out
    out="$("$JS2CT_BIN" "$file" --parse-only -o /dev/null 2>&1)"
    local status=$?

    local crashed=0
    if (( status >= 129 && status <= 192 )); then
        crashed=1
    fi

    if (( crashed )); then
        echo "FAIL  $name  (crashed, exit=$status -- likely killed by a signal)"
        echo "$out" | sed 's/^/        /'
        fail=$((fail + 1))
        fail_names+=("$name")
        return
    fi

    if [[ "$expect" == "good" ]]; then
        if (( status == 0 )); then
            echo "PASS  $name"
            pass=$((pass + 1))
        else
            echo "FAIL  $name  (expected to parse, but js2ct rejected it)"
            echo "$out" | sed 's/^/        /'
            fail=$((fail + 1))
            fail_names+=("$name")
        fi
    else
        if (( status != 0 )); then
            echo "PASS  $name"
            pass=$((pass + 1))
        else
            echo "FAIL  $name  (expected to be rejected, but js2ct parsed it)"
            fail=$((fail + 1))
            fail_names+=("$name")
        fi
    fi
}

shopt -s nullglob

echo "== lexer/good =="
for f in "$REPO_ROOT"/test/js2ct/lexer/good/*.js; do
    check_fixture "$f" good
done

echo "== lexer/bad =="
for f in "$REPO_ROOT"/test/js2ct/lexer/bad/*.js; do
    check_fixture "$f" bad
done

echo "== parser/good =="
for f in "$REPO_ROOT"/test/js2ct/parser/good/*.js; do
    check_fixture "$f" good
done

echo "== parser/bad =="
for f in "$REPO_ROOT"/test/js2ct/parser/bad/*.js; do
    check_fixture "$f" bad
done

shopt -u nullglob

echo
echo "$pass passed, $fail failed"

if [[ $fail -gt 0 ]]; then
    echo "failed: ${fail_names[*]}"
    exit 1
fi

exit 0
