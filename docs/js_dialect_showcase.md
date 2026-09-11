# JS dialect showcase: variables, objects, arrays, and functions

This document walks through small JavaScript programs and the actual
Cthulhu IR (`.ct`) that `js2ct` generates for them.

Each example is a real, checked-in, `assert`-verified fixture. Source pairs:

| JS source | Demonstrates |
|---|---|
| [reassign.js](../test/js2ct/e2e/reassign.js) | reassigning an existing variable to a new value |
| [field_mutation.js](../test/js2ct/e2e/field_mutation.js) | mutating a field on an object *without* reassigning the variable that holds it |
| [point.js](../test/js2ct/e2e/point.js) | object construction, string-keyed fields, a function call |
| [box.js](../test/js2ct/e2e/box.js) | an array nested inside an object |
| [sum_array.js](../test/js2ct/e2e/sum_array.js) | array iteration via a `while` loop |
| [counter.js](../test/js2ct/e2e/counter.js) | a loop that mutates an object field, across two function calls, plus a reassigned variable |

Run them yourself:

```bash
test/run_js2ct.sh
```

`run_js2ct.sh` chains `js2ct` → `ct2qjs` → `bcrun` for every fixture in
`test/js2ct/e2e/`; each one ends in a JS-level `assert(...)` that fails the
process (nonzero exit) if the computed answer is wrong. To see the Cthulhu
IR and bytecode for any one of them yourself, instead of trusting the text
below:

```bash
test/explore_js2ct.sh test/js2ct/e2e/point.js
```

## 1. Reassigning a variable — `reassign`

```js
function run() {
    let x = 1;
    x = x + 1;
    assert(x == 2);
}

run();
```

```
structure main
(
    run = λ
    (
        run run → f_ref1
        f__j call f_ref1 → %0
    )
)
structure run
(
    run = λ
    (
        jsvalue cons_1 → %0
        jsvalue dup %0 → %1 %2
        jsvalue cons_1 → %3
        jsvalue add %1 %3 → %4
        jsvalue drop → %2
        jsvalue copy %4 → %2
        jsvalue dup %2 → %5 %6
        jsvalue cons_2 → %7
        jsvalue eq? %5 %7 → %8
        jsvalue assert %8
        jsvalue drop → %6
    )
)
```

`cthu` is linear: every value is consumed exactly once, so there's no
mutable "variable slot" the way QuickJS bytecode has locals — a JS
binding is just a name attached to a value, and that name moves forward
through the instruction stream. A *fresh* `let x = 1;` is trivial —
`cons_1 → %0` and `%0` simply **is** `x` from here on, nothing to
reconcile with. *Re*assigning an existing binding (`x = x + 1;`) is the
interesting case: the RHS needs to read the old `x`, and the assignment
needs to produce a new value under the same name — but a linear value can
only be consumed once, so `x` (`%0`) is `dup`'d into two copies first:
one (`%1`) gets consumed normally as `add`'s operand; the other (`%2`) is
reserved as the write-back target. `drop → %2` then `copy %4 → %2` is the
reassignment itself — explicitly release the old value held under that
name, then claim the new one under it. This `drop`-then-`copy` pair (not
`move`, which only ever happens once, at a name's birth) is exactly what
distinguishes *re*assigning a binding from introducing a fresh one in
this IR — see `PLAN.md` P6.9. The final `assert(x == 2)` reads `x` (now
`%2`) the same way any read does: `dup` first, since the compiler doesn't
do liveness analysis to know `x` won't be read again — the unused second
copy (`%6`) is just `drop`ped at the end.

## 2. Mutating a field without reassigning the variable — `field_mutation`

```js
function run() {
    let obj = {};
    obj.x = 1;
    obj.x = obj.x + 1;
    assert(obj.x == 2);
}

run();
```

```
structure main
(
    run = λ
    (
        run run → f_ref1
        f__j call f_ref1 → %0
    )
)
structure run
(
    run = λ
    (
        jsvalue cons_obj → %0
        jsvalue cons_str "x" → %1
        jsvalue cons_1 → %2
        jsvalue dup %2 → %3 %4
        jsvalue set %0 %1 %3 → %5
        jsvalue cons_str "x" → %6
        jsvalue dup %5 → %7 %8
        jsvalue cons_str "x" → %9
        jsvalue get %7 %9 → %10
        jsvalue cons_1 → %11
        jsvalue add %10 %11 → %12
        jsvalue dup %12 → %13 %14
        jsvalue set %8 %6 %13 → %15
        jsvalue dup %15 → %16 %17
        jsvalue cons_str "x" → %18
        jsvalue get %16 %18 → %19
        jsvalue cons_2 → %20
        jsvalue eq? %19 %20 → %21
        jsvalue assert %21
        jsvalue drop → %17
    )
)
```

Contrast this with `reassign` above: **there's no `drop`/`copy` pair
anywhere.** `obj` the *binding* is never reassigned by the JS source —
only a field on it is. But `obj` still gets a fresh IR name after every
`set`: `%0` → `%5` → `%15`. That's not the compiler treating `obj.x =
...` like a variable reassignment — it falls straight out of `set`'s own
signature, `set :: obj × key × val → obj` (linear, like everything else
here): you feed `set` your only reference to the object, and it hands
back "the object, now mutated" as your new reference. The compiler just
updates its internal bookkeeping (the HIR-level environment entry for
`obj`) so the *next* read uses the right name — no explicit release
needed, because the old name (`%0`, then `%5`) was fully consumed by
being fed into `set`, not left dangling the way an orphaned value would
be. And it's not fiction: `dup` on a `jsvalue` is a QuickJS refcount
increment, not a deep copy (same reason `counter` below survives a full
round trip through loop mutation, unchanged in identity) — `%0`/`%5`/
`%15` are three IR *names* for what is, at runtime, the same underlying
heap object the whole time. One honest wrinkle visible here: `cons_str
"x"` appears twice (`%1`, then again at `%6`/`%9`) even though it's the
same literal both times — `js2ct` doesn't deduplicate repeated string-key
constants. Verbose, not wrong.

## 3. Object construction + a function call — `point`

```js
function makePoint(x, y) {
    let p = {};
    p.x = x;
    p.y = y;
    return p;
}

function run() {
    let p = makePoint(3, 4);
    assert(p.x + p.y == 7);
}

run();
```

```
structure main
(
    run = λ
    (
        run run → f_ref1
        f__j call f_ref1 → %0
    )
)
structure makePoint
(
    run = λ %0 %1 → out
    (
        jsvalue cons_obj → %2
        jsvalue cons_str "x" → %3
        jsvalue dup %0 → %4 %5
        jsvalue dup %4 → %6 %7
        jsvalue set %2 %3 %6 → %8
        jsvalue cons_str "y" → %9
        jsvalue dup %1 → %10 %11
        jsvalue dup %10 → %12 %13
        jsvalue set %8 %9 %12 → %14
        jsvalue dup %14 → %15 %16
        jsvalue move %15 → out
        jsvalue drop → %16
        jsvalue drop → %11
        jsvalue drop → %5
    )
)
structure run
(
    run = λ
    (
        jsvalue cons_3 → %0
        jsvalue cons_4 → %1
        makePoint run → f_ref1
        f_j2_j call f_ref1 %0 %1 → %2
        jsvalue dup %2 → %3 %4
        jsvalue cons_str "x" → %5
        jsvalue get %3 %5 → %6
        jsvalue dup %4 → %7 %8
        jsvalue cons_str "y" → %9
        jsvalue get %7 %9 → %10
        jsvalue add %6 %10 → %11
        jsvalue cons_7 → %12
        jsvalue eq? %11 %12 → %13
        jsvalue assert %13
        jsvalue drop → %8
    )
)
```

`makePoint` builds an object one `set` at a time (`cons_obj` starts empty,
each `set` returns the object again so the next field can chain off it),
then `move`s it out — `%0`/`%1` are `x`/`y`, each `dup`'d before being fed
to `set` since a parameter might be read again (here it isn't, hence the
trailing `drop`s cleaning up the unused second copies). `structure run`
(the JS function) calls `makePoint` the same way any named JS call lowers
(`<callee> run → f_ref` then `f_j2_j call f_ref args… → result`, `f_j2_j`
naming a 2-argument-1-result call shape — a pure naming convention, see
below), then reads `p.x` and `p.y` off the same object, which needs a
`dup` first: `get` consumes its object operand (`get :: obj × key →
value`, matching what the underlying `get_array_el` opcode actually
does), so getting a second field needs a second copy. `structure main` is
the compiler's own auto-generated script entry — it exists in every
`js2ct` program and just calls the JS-level `run()`.

## 4. An array nested inside an object — `box`

```js
function makeBox() {
    let b = {};
    let items = [];
    items[0] = 100;
    items[1] = 200;
    b.items = items;
    return b;
}

function run() {
    let b = makeBox();
    let items = b.items;
    assert(items[0] + items[1] == 300);
}

run();
```

```
structure main
(
    run = λ
    (
        run run → f_ref1
        f__j call f_ref1 → %0
    )
)
structure makeBox
(
    run = λ → out
    (
        jsvalue cons_obj → %0
        jsvalue cons_arr → %1
        jsvalue cons_0 → %2
        jsvalue cons_100 → %3
        jsvalue dup %3 → %4 %5
        jsvalue set %1 %2 %4 → %6
        jsvalue cons_1 → %7
        jsvalue cons_200 → %8
        jsvalue dup %8 → %9 %10
        jsvalue set %6 %7 %9 → %11
        jsvalue cons_str "items" → %12
        jsvalue dup %11 → %13 %14
        jsvalue dup %13 → %15 %16
        jsvalue set %0 %12 %15 → %17
        jsvalue dup %17 → %18 %19
        jsvalue move %18 → out
        jsvalue drop → %14
        jsvalue drop → %19
    )
)
structure run
(
    run = λ
    (
        makeBox run → f_ref1
        f__j call f_ref1 → %0
        jsvalue dup %0 → %1 %2
        jsvalue cons_str "items" → %3
        jsvalue get %1 %3 → %4
        jsvalue dup %4 → %5 %6
        jsvalue cons_0 → %7
        jsvalue get %5 %7 → %8
        jsvalue dup %6 → %9 %10
        jsvalue cons_1 → %11
        jsvalue get %9 %11 → %12
        jsvalue add %8 %12 → %13
        jsvalue cons_300 → %14
        jsvalue eq? %13 %14 → %15
        jsvalue assert %15
        jsvalue drop → %10
        jsvalue drop → %2
    )
)
```

The point of this one: `get`/`set`/`cons_obj`/`cons_arr` don't care what's
*inside* the value they're handling. `makeBox` builds `items` as an array
with integer keys (`cons_arr` + `set 0`/`set 1`) and then stores that
whole array under the string key `"items"` on a plain object (`set %0
"items" items`) — same two ops, no special-casing either way, because
`jsvalue` is a uniform runtime-tagged representation regardless of what's
stored where. `f__j` is the 0-argument call shape (`makeBox` takes no
parameters); `structure main` is again just the auto-generated driver
calling `run()`.

## 5. Array iteration — `sum_array`

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

function run() {
    let a = [];
    a[0] = 10;
    a[1] = 20;
    a[2] = 30;
    assert(sumArray(a, 3) == 60);
}

run();
```

```
structure main
(
    run = λ
    (
        run run → f_ref1
        f__j call f_ref1 → %0
    )
)
structure sumArray
(
    loop1 = λ %3 %2 %1 %0 → packed
    (
        jsvalue dup %2 → %4 %5
        jsvalue dup %1 → %6 %7
        jsvalue lt? %4 %6 → %8
        jsvalue dup %8 → cmp18 cmp19
        jsvalue not cmp19 → cmp20
        sumArray loopbody2 → ref21
        sumArray loopexit3 → ref22
        sumArray loopframe4 → ref23
        f_j4_j opt cmp18 ref21 → alt24
        f_j4_j opt cmp20 ref22 → alt25
        f_j4_j join alt24 alt25 ref23 → cont26
        f_j4_j call cont26 %3 %5 %7 %0 → packed
    )
    loopbody2 = λ %3 %2 %1 %0 → packed
    (
        jsvalue dup %3 → %9 %10
        jsvalue dup %0 → %11 %12
        jsvalue dup %2 → %13 %14
        jsvalue get %11 %13 → %15
        jsvalue add %9 %15 → %16
        jsvalue drop → %10
        jsvalue copy %16 → %10
        jsvalue dup %14 → %17 %18
        jsvalue cons_1 → %19
        jsvalue add %17 %19 → %20
        jsvalue drop → %18
        jsvalue copy %20 → %18
        sumArray loop1 → self14
        f_j4_j call self14 %10 %18 %1 %12 → packed
    )
    loopexit3 = λ %3 %2 %1 %0 → arr13
    (
        jsvalue cons_arr → arr5
        jsvalue cons_0 → k6
        jsvalue set arr5 k6 %3 → arr7
        jsvalue cons_1 → k8
        jsvalue set arr7 k8 %2 → arr9
        jsvalue cons_2 → k10
        jsvalue set arr9 k10 %1 → arr11
        jsvalue cons_3 → k12
        jsvalue set arr11 k12 %0 → arr13
    )
    loopframe4 = λ A B %3 %2 %1 %0 → packed17
    (
        jsvalue dup %3 → %3_1 %3_2
        jsvalue dup %2 → %2_1 %2_2
        jsvalue dup %1 → %1_1 %1_2
        jsvalue dup %0 → %0_1 %0_2
        f_j4_j call A %3_1 %2_1 %1_1 %0_1 → p115
        f_j4_j call B %3_2 %2_2 %1_2 %0_2 → p216
        jsvalue join p115 p216 → packed17
    )
    run = λ %0 %1 → out
    (
        jsvalue cons_0 → %2
        jsvalue cons_0 → %3
        sumArray loop1 → ref27
        f_j4_j call ref27 %3 %2 %1 %0 → packed28
        jsvalue dup packed28 → u29 u30
        jsvalue cons_0 → k31
        jsvalue get u29 k31 → %21
        jsvalue dup u30 → u32 u33
        jsvalue cons_1 → k34
        jsvalue get u32 k34 → %22
        jsvalue dup u33 → u35 u36
        jsvalue cons_2 → k37
        jsvalue get u35 k37 → %23
        jsvalue cons_3 → k38
        jsvalue get u36 k38 → %24
        jsvalue dup %21 → %25 %26
        jsvalue move %25 → out
        jsvalue drop → %26
        jsvalue drop → %22
        jsvalue drop → %23
        jsvalue drop → %24
    )
)
structure run
(
    run = λ
    (
        jsvalue cons_arr → %0
        jsvalue cons_0 → %1
        jsvalue cons_10 → %2
        jsvalue dup %2 → %3 %4
        jsvalue set %0 %1 %3 → %5
        jsvalue cons_1 → %6
        jsvalue cons_20 → %7
        jsvalue dup %7 → %8 %9
        jsvalue set %5 %6 %8 → %10
        jsvalue cons_2 → %11
        jsvalue cons_30 → %12
        jsvalue dup %12 → %13 %14
        jsvalue set %10 %11 %13 → %15
        jsvalue dup %15 → %16 %17
        jsvalue cons_3 → %18
        sumArray run → f_ref1
        f_j2_j call f_ref1 %16 %18 → %19
        jsvalue cons_60 → %20
        jsvalue eq? %19 %20 → %21
        jsvalue assert %21
        jsvalue drop → %17
    )
)
```

`while` lowers as self-recursion, not a jump: `loop1` re-checks the
condition on every call (initial or recursive) and dispatches between
`loopbody2` ("keep looping" — one iteration, then tail-calls `sumArray
loop1` again by name) and `loopexit3` ("loop is done") via the same
`opt`/`join`/`call` combinator `fib.ct` uses for plain recursion.
`loopframe4` is the `join` target: it calls both `opt`'d branches (one is
always a structural no-op, "bot") and merges their results — deliberate,
so abstract interpretation over this IR always sees both sides of every
branch, loop included.

**Why `packed` and not four named outputs.** `arr`/`n`/`i`/`total` (shown
as `%3`/`%2`/`%1`/`%0` here — the compiler orders a loop's live bindings
last-declared-first) are all live at loop entry, so all four have to
travel through every `call`/`opt`/`join` in the loop, in and out. A
QuickJS function can only ever `return` one value — there's no bytecode
for "return 4 things" — so instead of declaring four outputs, every
closure in the loop packs its four live values into one array before
returning (`loopexit3`'s `cons_arr`/`set` chain), and unpacks it again
wherever an individual value is needed next (`run`'s trailing `dup`/`get`
chain). `n`/`i`/`arr` are dead once the loop exits, so `run` just `drop`s
them after unpacking and keeps `total` (`%21`). This packing is the fix
for a real bug found while building an earlier, hand-written version of
this same example (`PLAN.md`'s P2.5): an earlier codegen path declared
separate outputs directly on the call and silently wrote only the first
one — exactly the kind of bug a single passing run won't reveal.

Also visible here for the first time: `loopbody2`'s `jsvalue drop → %10`
/ `jsvalue copy %16 → %10` pair, right after computing the new `total`.
That's `qjs_val_copy` (`PLAN.md` P6.9) — JS's `total = total + arr[i]`
reassigns an *existing* binding, which lowers as drop-the-old-value then
copy-the-new-value-into-that-slot, distinct from `move` (used for a
fresh `let`). Before this was implemented, `sumArray`'s literal source
couldn't compile through `js2ct` at all.

## 6. A loop that mutates an object field — `counter`

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

function run() {
    let c = makeCounter();
    c = incrementBy(c, 5);
    assert(c.count == 5);
}

run();
```

```
structure main
(
    run = λ
    (
        run run → f_ref1
        f__j call f_ref1 → %0
    )
)
structure makeCounter
(
    run = λ → out
    (
        jsvalue cons_obj → %0
        jsvalue cons_str "count" → %1
        jsvalue cons_0 → %2
        jsvalue dup %2 → %3 %4
        jsvalue set %0 %1 %3 → %5
        jsvalue dup %5 → %6 %7
        jsvalue move %6 → out
        jsvalue drop → %7
    )
)
structure incrementBy
(
    loop1 = λ %2 %1 %0 → packed
    (
        jsvalue dup %2 → %3 %4
        jsvalue dup %1 → %5 %6
        jsvalue lt? %3 %5 → %7
        jsvalue dup %7 → cmp16 cmp17
        jsvalue not cmp17 → cmp18
        incrementBy loopbody2 → ref19
        incrementBy loopexit3 → ref20
        incrementBy loopframe4 → ref21
        f_j3_j opt cmp16 ref19 → alt22
        f_j3_j opt cmp18 ref20 → alt23
        f_j3_j join alt22 alt23 ref21 → cont24
        f_j3_j call cont24 %4 %6 %0 → packed
    )
    loopbody2 = λ %2 %1 %0 → packed
    (
        jsvalue cons_str "count" → %8
        jsvalue dup %0 → %9 %10
        jsvalue cons_str "count" → %11
        jsvalue get %9 %11 → %12
        jsvalue cons_1 → %13
        jsvalue add %12 %13 → %14
        jsvalue dup %14 → %15 %16
        jsvalue set %10 %8 %15 → %17
        jsvalue dup %2 → %18 %19
        jsvalue cons_1 → %20
        jsvalue add %18 %20 → %21
        jsvalue drop → %19
        jsvalue copy %21 → %19
        incrementBy loop1 → self12
        f_j3_j call self12 %19 %1 %17 → packed
    )
    loopexit3 = λ %2 %1 %0 → arr11
    (
        jsvalue cons_arr → arr5
        jsvalue cons_0 → k6
        jsvalue set arr5 k6 %2 → arr7
        jsvalue cons_1 → k8
        jsvalue set arr7 k8 %1 → arr9
        jsvalue cons_2 → k10
        jsvalue set arr9 k10 %0 → arr11
    )
    loopframe4 = λ A B %2 %1 %0 → packed15
    (
        jsvalue dup %2 → %2_1 %2_2
        jsvalue dup %1 → %1_1 %1_2
        jsvalue dup %0 → %0_1 %0_2
        f_j3_j call A %2_1 %1_1 %0_1 → p113
        f_j3_j call B %2_2 %1_2 %0_2 → p214
        jsvalue join p113 p214 → packed15
    )
    run = λ %0 %1 → out
    (
        jsvalue cons_0 → %2
        incrementBy loop1 → ref25
        f_j3_j call ref25 %2 %1 %0 → packed26
        jsvalue dup packed26 → u27 u28
        jsvalue cons_0 → k29
        jsvalue get u27 k29 → %22
        jsvalue dup u28 → u30 u31
        jsvalue cons_1 → k32
        jsvalue get u30 k32 → %23
        jsvalue cons_2 → k33
        jsvalue get u31 k33 → %24
        jsvalue dup %24 → %25 %26
        jsvalue move %25 → out
        jsvalue drop → %22
        jsvalue drop → %23
        jsvalue drop → %26
    )
)
structure run
(
    run = λ
    (
        makeCounter run → f_ref1
        f__j call f_ref1 → %0
        jsvalue dup %0 → %1 %2
        jsvalue cons_5 → %3
        incrementBy run → f_ref2
        f_j2_j call f_ref2 %1 %3 → %4
        jsvalue drop → %2
        jsvalue copy %4 → %2
        jsvalue dup %2 → %5 %6
        jsvalue cons_str "count" → %7
        jsvalue get %5 %7 → %8
        jsvalue cons_5 → %9
        jsvalue eq? %8 %9 → %10
        jsvalue assert %10
        jsvalue drop → %6
    )
)
```

Same loop shape as `sum_array` (three live bindings this time — `c`,
`n`, `i` — so `f_j3_j`, packing three values instead of four), but the
payload being threaded is an object reference, not a number, and
`loopbody2` mutates it via read-modify-write (`get "count"` → `add 1` →
`set "count"`) on every iteration rather than just reading it.
`structure run` (the JS function `run()`, the whole program's driver)
chains two calls — `makeCounter` then `incrementBy` — and its own
`c = incrementBy(c, 5);` is `qjs_val_copy` again, visible as `jsvalue
drop → %2` / `jsvalue copy %4 → %2` right after the call: `c` is an
*existing* binding being reassigned to the call's result, not a fresh
`let`. The object survives a full round trip through construction, five
loop iterations of field mutation, a reassignment, and a final field
read, unchanged in identity throughout (QuickJS's `dup` opcode is a
refcount increment for heap values, not a copy — the same mechanism that
makes any of this work with zero new `cthu` types).
