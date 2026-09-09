# JS dialect showcase: objects, arrays, and functions

This document walks through four small JavaScript programs and the
hand-written Cthulhu IR (`.ct`) that represents them, to make concrete what
Block 3's memory-model work (`docs/project_plan.md`) actually compiles to.

Each example is a real, checked-in, `assert`-verified fixture — not
pseudocode. Source pairs:

| JS source | Cthulhu IR | Demonstrates |
|---|---|---|
| [point.js](../test/ct2qjs/point.js) | [point.ct](../test/ct2qjs/point.ct) | object construction, string-keyed fields, a function call |
| [box.js](../test/ct2qjs/box.js) | [box.ct](../test/ct2qjs/box.ct) | an array nested inside an object |
| [sum_array.js](../test/ct2qjs/sum_array.js) | [sum_array.ct](../test/ct2qjs/sum_array.ct) | array iteration via a `while` loop |
| [counter.js](../test/ct2qjs/counter.js) | [counter.ct](../test/ct2qjs/counter.ct) | a loop that mutates an object field, across two function calls |

Run them yourself:

```bash
test/run.sh
```

`run.sh` compiles every `.ct` fixture in `test/ct2qjs/` with `cthuc` and
runs the result with `bcrun`; each of these four ends in an `assert` that
fails the process (nonzero exit) if the computed answer is wrong.

**Why hand-written, not `js2ct` output:** `js2ct`'s JS→AST/HIR pipeline
doesn't parse object/array literals or member access yet (see
`Claude.md`'s "not yet covered" list) — objects and arrays exist today at
the `cthu`/`cthuc` level only (Block 3's actual deliverable). These
fixtures show what `js2ct` should eventually emit for this JS, written by
hand against the same lowering conventions `js2ct` already uses for
functions, calls, and loops.

## The memory model in one paragraph

`jsvalue` already *is* QuickJS's `JSValue`, which already unifies scalars,
objects, and arrays — so no new `cthu` type and no new signature category
was needed. Four new ops on the existing `jsvalue` structure:
`cons_obj`/`cons_arr` (construct, committing to a kind up front) and a
shared `get`/`set` (dispatch on the value's own runtime tag, so the same
two ops work for both object and array access). `set` isn't a single
opcode — QuickJS's `put_array_el` consumes its operands and returns
nothing, but JS's `obj[k] = v` evaluates to `v`, not `obj` — so `set` dups
the object, feeds one copy to the store, and hands back the surviving
copy. See [codegen.cpp](../src/ct2qjs/codegen.cpp) (`qjs_val_cons_obj` /
`qjs_val_cons_arr` / `qjs_val_get` / `qjs_val_set`).

## 1. Object construction + a function call — `point`

```js
function makePoint(x, y) {
    let p = {};
    p.x = x;
    p.y = y;
    return p;
}
function main() {
    let p = makePoint(3, 4);
    return p.x + p.y;
}
```

```
structure makePoint
(
    run = λ x y → out
    (
        jsvalue cons_obj → p0
        jsvalue cons_str "x" → kx
        jsvalue set p0 kx x → p1

        jsvalue cons_str "y" → ky
        jsvalue set p1 ky y → p2

        jsvalue move p2 → out
    )
)

structure main
(
    run = λ
    (
        jsvalue cons_3 → three
        jsvalue cons_4 → four

        makePoint run                → mkref
        f_jj_j call mkref three four → p

        jsvalue dup p → p1 p2
        jsvalue cons_str "x" → kx
        jsvalue get p1 kx → xval

        jsvalue cons_str "y" → ky
        jsvalue get p2 ky → yval

        jsvalue add xval yval → total
        jsvalue cons_7 → expect
        jsvalue eq? total expect → ok
        jsvalue assert ok
    )
)
```

`makePoint` builds an object one `set` at a time (`cons_obj` starts empty,
each `set` returns the object again so the next field can chain off it),
then `move`s it out. `main` calls it (every JS function becomes its own
`structure`; calling one is `<callee> run → f_ref` then `f_jj_j call f_ref
args… → out` — `f_jj_j` names a 2-argument, 1-result call shape, a pure
naming convention with no declaration anywhere, see `Claude.md`). Reading
`p.x` and `p.y` both off the same object needs a `dup` first: `get`
consumes its object operand (`get :: obj × key → value`, matching what the
underlying `get_array_el` opcode actually does), so getting a second field
needs a second copy.

## 2. An array nested inside an object — `box`

```js
function makeBox() {
    let b = {};
    let items = [];
    items[0] = 100;
    items[1] = 200;
    b.items = items;
    return b;
}
function main() {
    let b = makeBox();
    let items = b.items;
    return items[0] + items[1];
}
```

```
structure makeBox
(
    run = λ → out
    (
        jsvalue cons_arr → items0
        jsvalue cons_0   → k0
        jsvalue cons_100 → v0
        jsvalue set items0 k0 v0 → items1

        jsvalue cons_1   → k1
        jsvalue cons_200 → v1
        jsvalue set items1 k1 v1 → items2

        jsvalue cons_obj → b0
        jsvalue cons_str "items" → key
        jsvalue set b0 key items2 → b1

        jsvalue move b1 → out
    )
)

structure main
(
    run = λ
    (
        makeBox run      → mkref
        f__j call mkref  → b0

        jsvalue cons_str "items" → key
        jsvalue get b0 key → items

        jsvalue dup items → items1 items2
        jsvalue cons_0 → k0
        jsvalue get items1 k0 → v0

        jsvalue cons_1 → k1
        jsvalue get items2 k1 → v1

        jsvalue add v0 v1 → total
        jsvalue cons_300 → expect
        jsvalue eq? total expect → ok
        jsvalue assert ok
    )
)
```

The point of this one: `get`/`set`/`cons_obj`/`cons_arr` don't care what's
*inside* the value they're handling. An array built with integer keys
(`cons_arr` + `set 0`/`set 1`) becomes exactly as valid a value under a
string-keyed object field (`set b0 "items" items2`) as a plain number
would — same ops, no special-casing, because `jsvalue` is a uniform
runtime-tagged representation both ways. `f__j` here is the 0-argument
call shape (`makeBox` takes no parameters).

## 3. Array iteration — `sum_array`

```js
function sumArray(arr, n) {
    let i = 0;
    let total = 0;
    while (i < n) {
        total = total + arr[i];
        i = i + 1;
    }
    return total;
}
function main() {
    let a = [];
    a[0] = 10; a[1] = 20; a[2] = 30;
    return sumArray(a, 3);
}
```

```
structure sumArray
(
    body_0 = λ arr n i total → packed
    (
        jsvalue dup arr → arr1 arr2
        jsvalue dup i   → i1 i2
        jsvalue get arr1 i1 → elem

        jsvalue add total elem → total2

        jsvalue cons_1  → one
        jsvalue add i2 one → inext

        sumArray loop_0                      → self
        f_j4_j call self arr2 n inext total2 → packed
    )

    exit_0 = λ arr n i total → packed
    (
        jsvalue cons_arr → p0
        jsvalue cons_0   → k0
        jsvalue set p0 k0 arr → p1
        jsvalue cons_1   → k1
        jsvalue set p1 k1 n → p2
        jsvalue cons_2   → k2
        jsvalue set p2 k2 i → p3
        jsvalue cons_3   → k3
        jsvalue set p3 k3 total → packed
    )

    frame_0 = λ A B arr n i total → packed
    (
        jsvalue dup arr   → arr_1 arr_2
        jsvalue dup n     → n_1 n_2
        jsvalue dup i     → i_1 i_2
        jsvalue dup total → total_1 total_2

        f_j4_j call A arr_1 n_1 i_1 total_1 → p1
        f_j4_j call B arr_2 n_2 i_2 total_2 → p2
        jsvalue join p1 p2                  → packed
    )

    loop_0 = λ arr n i total → packed
    (
        jsvalue dup i → i1 i2
        jsvalue dup n → n1 n2
        jsvalue lt? i1 n1 → cmp

        jsvalue dup cmp  → cmp1 cmp2
        jsvalue not cmp2 → cmp3

        sumArray body_0  → body_ref
        sumArray exit_0  → exit_ref
        sumArray frame_0 → frame

        f_j4_j opt cmp1 body_ref → alt1
        f_j4_j opt cmp3 exit_ref → alt2
        f_j4_j join alt1 alt2 frame → cont

        f_j4_j call cont arr n2 i2 total → packed
    )

    run = λ arr n → out
    (
        jsvalue cons_0 → i0
        jsvalue cons_0 → total0

        sumArray loop_0                  → cont
        f_j4_j call cont arr n i0 total0 → packed

        jsvalue dup packed → u1 u2
        jsvalue cons_0     → uk0
        jsvalue get u1 uk0 → r_arr
        jsvalue dup u2     → u3 u4
        jsvalue cons_1     → uk1
        jsvalue get u3 uk1 → r_n
        jsvalue dup u4     → u5 u6
        jsvalue cons_2     → uk2
        jsvalue get u5 uk2 → r_i
        jsvalue cons_3     → uk3
        jsvalue get u6 uk3 → r_total

        jsvalue drop r_arr
        jsvalue drop r_n
        jsvalue drop r_i
        jsvalue move r_total → out
    )
)
```

`while` lowers as self-recursion, not a jump: `loop_0` re-checks the
condition on every call (initial or recursive) and dispatches between
`body_0` ("keep looping" — one iteration, then tail-calls `sumArray
loop_0` again by name) and `exit_0` ("loop is done") via the same
`opt`/`join`/`call` combinator `fib.ct` uses for plain recursion.
`frame_0` is the `join` target: it calls both `opt`'d branches (one is
always a structural no-op, "bot") and merges their results — this is
deliberate, so abstract interpretation over this IR always sees both sides
of every branch, loop included.

**Why `packed` and not four named outputs.** `arr`, `n`, `i`, `total` are
all live at loop entry, so all four have to travel through every
`call`/`opt`/`join` in the loop, in and out. A QuickJS function can only
ever `return` one value — there's no bytecode for "return 4 things" — so
instead of trying to declare four outputs (which silently only propagates
the first one; see below), every closure in the loop packs its four live
values into one array before returning, and unpacks it again wherever an
individual value is needed next. That's `exit_0`'s `cons_arr`/`set` chain
and `run`'s trailing `dup`/`get` chain. `n`/`i`/`arr` are dead once the
loop exits, so `run` just `drop`s them after unpacking and keeps `total`.

This packing is not a stylistic choice — it's the fix for a real bug
(`docs/../PLAN.md`'s P2.5): an earlier version of this loop shape declared
four separate outputs directly on the call, and `codegen.hpp` silently
wrote only the first one, leaving the rest as whatever was already in that
QuickJS local slot. It happened to produce the right answer whenever slot
reuse coincidentally placed the needed value at position 0 — which is
exactly the kind of bug a single passing test run won't reveal, and didn't
here, until a second test with a different variable-position count broke
differently on every rebuild.

## 4. A loop that mutates an object field — `counter`

```js
function makeCounter() {
    let c = {};
    c.count = 0;
    return c;
}
function incrementBy(c, n) {
    let i = 0;
    while (i < n) {
        c.count = c.count + 1;
        i = i + 1;
    }
    return c;
}
function main() {
    let c = makeCounter();
    c = incrementBy(c, 5);
    return c.count;
}
```

```
structure makeCounter
(
    run = λ → out
    (
        jsvalue cons_obj → c0
        jsvalue cons_str "count" → key
        jsvalue cons_0 → zero
        jsvalue set c0 key zero → c1
        jsvalue move c1 → out
    )
)

structure incrementBy
(
    body_0 = λ c n i → packed
    (
        jsvalue dup c → c1 c2
        jsvalue cons_str "count" → key1
        jsvalue get c1 key1 → cnt

        jsvalue cons_1 → one
        jsvalue add cnt one → cnt2

        jsvalue cons_str "count" → key2
        jsvalue set c2 key2 cnt2 → c3

        jsvalue dup i → i1 i2
        jsvalue cons_1 → one2
        jsvalue add i2 one2 → inext

        incrementBy loop_0          → self
        f_j3_j call self c3 n inext → packed
    )

    exit_0 = λ c n i → packed
    (
        jsvalue cons_arr → p0
        jsvalue cons_0   → k0
        jsvalue set p0 k0 c → p1
        jsvalue cons_1   → k1
        jsvalue set p1 k1 n → p2
        jsvalue cons_2   → k2
        jsvalue set p2 k2 i → packed
    )

    frame_0 = λ A B c n i → packed
    (
        jsvalue dup c → c_1 c_2
        jsvalue dup n → n_1 n_2
        jsvalue dup i → i_1 i_2

        f_j3_j call A c_1 n_1 i_1 → p1
        f_j3_j call B c_2 n_2 i_2 → p2
        jsvalue join p1 p2         → packed
    )

    loop_0 = λ c n i → packed
    (
        jsvalue dup i → i1 i2
        jsvalue dup n → n1 n2
        jsvalue lt? i1 n1 → cmp

        jsvalue dup cmp  → cmp1 cmp2
        jsvalue not cmp2 → cmp3

        incrementBy body_0  → body_ref
        incrementBy exit_0  → exit_ref
        incrementBy frame_0 → frame

        f_j3_j opt cmp1 body_ref → alt1
        f_j3_j opt cmp3 exit_ref → alt2
        f_j3_j join alt1 alt2 frame → cont

        f_j3_j call cont c n2 i2 → packed
    )

    run = λ c n → out
    (
        jsvalue cons_0 → i0

        incrementBy loop_0      → cont
        f_j3_j call cont c n i0 → packed

        jsvalue dup packed → u1 u2
        jsvalue cons_0     → uk0
        jsvalue get u1 uk0 → r_c
        jsvalue dup u2     → u3 u4
        jsvalue cons_1     → uk1
        jsvalue get u3 uk1 → r_n
        jsvalue cons_2     → uk2
        jsvalue get u4 uk2 → r_i

        jsvalue drop r_n
        jsvalue drop r_i
        jsvalue move r_c → out
    )
)

structure main
(
    run = λ
    (
        makeCounter run → mkref
        f__j call mkref  → c0

        jsvalue cons_5 → five

        incrementBy run            → incref
        f_jj_j call incref c0 five → c1

        jsvalue cons_str "count" → key
        jsvalue get c1 key → cnt

        jsvalue cons_5 → expect
        jsvalue eq? cnt expect → ok
        jsvalue assert ok
    )
)
```

Same loop shape as `sum_array` (three live bindings this time — `c`, `n`,
`i` — so `f_j3_j`, packing three values instead of four), but the payload
being threaded is an object reference, not a number, and `body_0` mutates
it via read-modify-write (`get "count"` → `add 1` → `set "count"`) on
every iteration rather than just reading it. `main` chains two calls —
`makeCounter` then `incrementBy` — showing the object survives a full
round trip through construction, five loop iterations of field mutation,
and a final field read, unchanged in identity throughout (QuickJS's `dup`
opcode is a refcount increment for heap values, not a copy — the same
mechanism that makes any of this work with zero new `cthu` types).

## Naming convention: `f_j..._j`

Every `call`/`opt`/`join` above targets a "structure" like `f_jj_j` or
`f_j4_j`. This is a pure naming convention — `ir.hpp::classify()` resolves
these three operations by *operation name alone*, so `f_j4_j` needs no
`structure f_j4_j ( ... )` declared anywhere. The letter is `j` because
every argument and the single result here are `jsvalue`; a run of `k`
identical letters compacts to `<letter><k>` (`jjj` → `j3`), and the suffix
is always a single `_j` because a call can only ever produce one output
(see the `packed` discussion above) — there is no `f_..._jj` shape.
