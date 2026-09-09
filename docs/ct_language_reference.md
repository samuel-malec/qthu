# The `.ct` language and QuickJS backend

This document describes the Cthulhu intermediate language (`.ct`) as
`cthuc` (source: `src/ct2qjs/`) actually parses and compiles it today, and
how each construct maps onto QuickJS bytecode. It is written for a reader
who has not read `codegen.cpp`, not as a design-intent or TODO document —
see `Claude.md` for open bugs and `PLAN.md`/`STATUS.md` for the project's
task history. Everything below is checked against the current source, not
the original thesis design (`Vladimír Uhlík — Compiling C to Cthulhu`),
which this project diverges from in several places noted inline.

## 1. Pipeline

```
JavaScript --js2ct--> Cthulhu IR (.ct text) --cthuc--> QuickJS bytecode
```

`js2ct` and `cthuc` are separate, independently runnable programs. This
document covers only the `.ct` language itself and `cthuc`'s compilation
of it — not `js2ct`'s JS-to-`.ct` lowering (see `Claude.md`'s "JS dialect
lowering conventions" for that).

## 2. The `.ct` grammar

A `.ct` file is a flat sequence of top-level declarations, each starting
with a keyword and ending with a parenthesized body:

```
file       ::= declaration*
declaration::= type_decl | signature_decl | structure_decl

type_decl      ::= "type" IDENT
signature_decl ::= "signature" IDENT "[" IDENT ("," IDENT)* "]" (":" inherit ("," inherit)*)? "(" sig_def* ")"
inherit        ::= IDENT "[" IDENT ("," IDENT)* "]"
sig_def        ::= IDENT "∷" IDENT ("×" IDENT)* "→" IDENT ("×" IDENT)*

structure_decl ::= "structure" IDENT (":" inherit ("," inherit)*)? "(" struct_member* ")"
struct_member  ::= IDENT "=" (IDENT | lambda)
lambda         ::= "λ" IDENT* ("→" IDENT*)? "(" instruction* ")"
instruction    ::= IDENT IDENT (STRING | IDENT*) ("→" IDENT+)?
```

Notes that aren't obvious from the grammar alone:

- **`type` declarations are phantom.** `type stck` just registers a name;
  nothing about it is ever checked. Types only appear as signature type
  parameters (`arithmetic[jsvalue, stck, bool]`) — there's no
  type-checker, so a type that's never given a concrete backing structure
  (like `stck`, see `PLAN.md` P4.1) is not an error.
- **`signature` blocks are pure interface documentation.** `cthuc` never
  verifies that a `structure : some_signature[...]` actually binds every
  operation `some_signature` declares (`Claude.md`: "signatures are
  documentation, not enforced yet"). A structure can bind a subset, or
  bind operations no inherited signature ever declared, and `cthuc` won't
  complain either way — only instructions actually present in some
  lambda's body ever need to resolve to something.
- **A `struct_member` is either a builtin binding (`name = other_ident`)
  or a lambda (`name = λ ...`).** The right-hand identifier of a builtin
  binding is looked up by `emit_builtin` at codegen time (see §5) — it
  isn't itself a `.ct`-level name, it's a string `codegen.cpp` switches on
  (`qjs_val_add`, `qjs_val_cons_obj`, etc.).
- **An instruction's first two identifiers are `structure operation`**,
  e.g. `jsvalue add a b → c` (structure = `jsvalue`, operation = `add`) or
  `f_jj_j call f_ref x y → out` (structure = `f_jj_j`, operation =
  `call`). How that pair resolves is the single most important mechanical
  fact about this language — see §4.
- **String-literal operands** (`jsvalue cons_str "count" → key`) are the
  one instruction shape without a plain identifier list between the
  operation and the arrow; `reader.cpp`'s `read_function` special-cases a
  `token::str` there and stores it on `insn_t::literal` instead of `in`.
- **`cons_N` for any integer `N`, and `cons_true`/`cons_false`, need no
  `builtins.ct` entry.** `ir.hpp::classify()` auto-registers any
  `jsvalue cons_<suffix>` instruction as a builtin named
  `qjs_val_cons_<suffix>` the first time it's seen (`classify()` treats
  `cons_obj`/`cons_arr`/`cons_str` no differently from `cons_5` at this
  stage — they all just auto-register as `qjs_val_cons_<suffix>`), and
  `codegen.cpp`'s `emit_builtin` has a generic `qjs_val_cons_` prefix
  handler that parses `<suffix>` at codegen time (`std::from_chars`, or
  the literal strings `true`/`false`). This is why `jsvalue cons_5 → x`
  and `jsvalue cons_9999 → x` both just work with no declaration anywhere
  — but it's `emit_builtin`'s own `if`-chain, not `classify()`, that keeps
  `cons_obj`/`cons_arr`/`cons_str` working: their exact-match cases are
  placed *before* the generic prefix handler in that chain, since
  otherwise they'd reach it first and be rejected as "not a number".

### 2.1 Worked example

```
structure jsvalue : arithmetic[ jsvalue, stck, bool ]
(
    add = qjs_val_add        ; builtin binding
    dup = qjs_val_dup

    run = λ x y → out         ; a lambda: two inputs, one output
    (
        jsvalue dup x → x1 x2  ; instruction: structure=jsvalue, op=dup
        jsvalue add x1 y → s
        jsvalue move s → out
    )
)
```

Every identifier inside a lambda body (`x`, `x1`, `s`, `out`, ...) is a
**value name**, scoped to that one lambda — see §3.

## 3. Values, slots, and the linear discipline

Every `.ct` value is, at the QuickJS level, a `JSValue` (`jsvalue` is
literally QuickJS's own tagged union — see `Claude.md`'s "no new type"
notes throughout the memory-model work). `cthuc` never introduces a
separate boxed representation for arrays, objects, strings, or numbers.

**Slot allocation** (`ir.hpp::aloc_slots()`) walks each lambda's
instruction list once and assigns every value name a local-variable slot
number, with a stack-discipline reuse rule:

1. A lambda's own parameters get slots first, in order.
2. For each instruction, every *input* name's slot is looked up, recorded,
   and immediately marked free (pushed onto a free list) — **before** any
   of that instruction's own outputs are allocated.
3. Each *output* name gets a slot: reused from the free list if one is
   available (LIFO), otherwise a fresh one.
4. A function's own declared outputs (the names after `→` on the lambda
   signature line) are resolved by looking up their current slot once the
   whole body has been walked.

Two consequences worth knowing, both because they've caused real bugs
(`PLAN.md` P4.1, P2.5):

- **An input name is consumed exactly once.** Reusing a value after it's
  been an instruction's input without an intervening `dup` is a
  compile-time `slot alloc underflow`, not a silent bug — this is the
  actual enforcement of "linear" in "linear IR" today (there is no
  separate linearity checker; slot allocation *is* the check).
- **An instruction's own output slot can alias one of its own input
  slots**, per rule 2 above. This is fine as long as a builtin's codegen
  case reads every input it needs *before* writing any output — writing
  an output early can silently corrupt an input it still needs to read
  again later in the same case (see `qjs_val_pop`'s fix, `PLAN.md` P4.1,
  for a real example and the general shape of this bug class).

`dup`/`drop`/`move` are the primitive vocabulary for working within this
discipline: `dup` (QuickJS's own `dup` opcode, which increments a heap
value's refcount — not a deep copy) turns one owned reference into two;
`drop` releases one; `move` renames one without touching it at the
bytecode level at all (`qjs_val_move`'s codegen is just `get_loc`/
`put_loc`, no QuickJS-level op in between).

## 4. Control flow: `opt` / `join` / `call`

There is no jump/label-based control flow in this IR, and no CFG.
Branches and loops are represented as ordinary structure members (named
lambdas — closures, at the bytecode level) selected and invoked through
three operations that `ir.hpp::classify()` resolves **by operation name
alone**, before any builtin/lambda lookup:

```cpp
if ( op_name == "call" )                              -> fn_call
if ( op_name == "opt" )                                -> fn_opt
if ( op_name == "join" && structure_name starts "f" )  -> fn_join
```

This is why a call-arity "structure" name like `f_jj_j` or `f_j3_j` needs
**no declaration anywhere in the `.ct` file** — `call`/`opt`/`join`
targeting it resolve mechanically, regardless of what (if anything) that
identifier otherwise names. The convention (every `.ct` file in this repo
follows it, but nothing enforces it) is documented in `Claude.md` §"Core
cthu model".

`jsvalue join a b → c` (two arguments, structure = `jsvalue`, not
`f...`) is a *different* operation from `f_jj_j join a b c → d` (three
arguments, structure starts with `f`) — the same source keyword resolves
to two unrelated codegen paths (`qjs_val_join`, a builtin, vs. `fn_join`,
below) purely based on argument count and the structure name's first
letter. Do not confuse them when reading a `.ct` file.

### 4.1 What each one actually compiles to

**`opt :: B × F → F`** (`emit_fn_opt`, `codegen.cpp`): reads the boolean,
and if true, forwards the closure reference (`F`) unchanged; if false,
produces `undefined`. `undefined` *is* the "bot"/no-op closure from the
theoretical model — there's no separate sentinel value.

```
cmp1 ? then_ref : undefined   → alt1
```

**`join :: F × F × F → F`** (`emit_fn_join`, `codegen.cpp`) — **note the
code's own comment: "this is not the semantics of cthulhu join, it is
done this way to simplify the codegen".** The real cthulhu model (per
`Claude.md`) is: `join` partially applies a hand-written `frame`
combinator, binding both `opt`'d branches into one closure that
*structurally* invokes both sides, so static/abstract analysis always
sees both branches. What `emit_fn_join` actually does at the bytecode
level is much simpler and is a genuine, deliberate deviation from that
model:

```
if alt1 is not undefined: use alt1
else if alt2 is not undefined: use alt2
else: throw
```

The third argument (`frame`) is **read as an operand but never invoked**
by this codegen. `frame_N`'s own lambda body (which really does call both
branches and `jsvalue join` their two results — see §4.2) is compiled and
present in the output bytecode, but at runtime the picked branch (`alt1`
or `alt2`) is called directly; `frame_N` is dead code from the QuickJS
interpreter's point of view. This is a real, load-bearing simplification,
not an oversight — flagging it here because it's easy to read `frame_N`'s
own body, assume it's what actually executes, and be wrong about where
the real control-flow decision happens.

**`call :: F × I → O`** (`fn_call` case, `codegen.hpp`'s `gen_fn`): the
one place a value produced by `opt`/`join` (or a plain `struct_name
lambda_name → ref` reference) is actually invoked — `get_loc` the closure
reference and every argument, in order, then the QuickJS `call` opcode.
**Only ever propagates one output** (`insn.slots_out[0]`) — real QuickJS
functions return exactly one value, so a `.ct` function/call declaring
more than one output silently drops every output past the first (see
`PLAN.md` P2.5: this is why a loop's N live bindings are packed into a
single array before any `call`/`opt`/`join`, not passed as N separate
declared outputs).

### 4.2 A full example: tail-position `if`

`then_N`/`else_N`/`frame_N`/`opt`/`join`/`call`, the pattern every
`if`/`while`/`do-while` in this codebase reduces to (this is `fib.ct`'s
`base`/`rec`/`frame`/`loop`, `02_if_tail.ct`'s `then_0`/`else_0`, and
every loop's `loopbody_N`/`loopexit_N`/`loopframe_N`/`loop_N` — same
shape throughout):

```
abs then_0  → then_ref     ; struct_name lambda_name → ref (a plain
abs else_0  → else_ref     ; reference to a sibling closure — resolved
abs frame_0 → frame        ; the same way any other fn_ref is)

f_j_j opt cmp1 then_ref → alt1     ; alt1 = (x<0) ? then_ref : undefined
f_j_j opt cmp3 else_ref → alt2     ; alt2 = !(x<0) ? else_ref : undefined
f_j_j join alt1 alt2 frame → cont  ; cont = whichever of alt1/alt2 isn't undefined

f_j_j call cont x2 → out           ; actually invoke it
```

`frame_0`'s own body (never invoked at runtime, per §4.1) shows the
*theoretical* both-branches-explored shape: `dup` every parameter, `call`
both `A` and `B` (the two closures `join` was given), then merge the two
results with the plain 2-arg `jsvalue join`:

```
frame_0 = λ A B x → out
(
    jsvalue dup x → x1 x2
    f_j_j call A x1 → out1
    f_j_j call B x2 → out2
    jsvalue join out1 out2 → out
)
```

## 5. Structures, closures, and captures

At the bytecode level, a `.ct` `structure`'s named lambdas are not nested
functions or methods — they're **sibling closures sharing one flat
top-level scope**. `gen_root()` (`codegen.hpp`) emits one `fclosure` +
`put_loc` pair per compiled function into a synthetic `__toplevel__`
QuickJS function, in declaration order; `struct_name lambda_name → ref`
resolves to a `get_var_ref_` read of whichever `__toplevel__` local that
lambda was stored into.

`capture_root_locals()` scans a lambda's body for every `fn_ref` it
contains and captures exactly those functions as QuickJS closure
variables — this is *why* a lambda can reference itself by name (`sumTo
loop_0 → self` inside `loop_0`'s own body) with zero special-casing:
self-reference is just an `fn_ref` like any other, and by the time any
lambda body is being resolved, every structure's full function table is
already populated in the symbol table (`Claude.md`'s "mechanical
discovery" note).

## 6. Builtin operations: `.ct` name → `qjs_val_*` → QuickJS opcode

Every row below is a `builtins.ct` binding (`.ct`-facing operation name =
`qjs_val_*` string) and the `codegen.cpp` case that string dispatches to
(`emit_builtin`, switching on `ir.st.name_of(insn.resolved.target)`).
"Opcode(s)" lists the QuickJS bytecode ops actually emitted, in order,
not counting the `get_loc`/`put_loc` pairs that move operands to and from
local slots (every case does some of those; they're omitted here for
readability — §3 covers what they mean).

| `.ct` op | `qjs_val_*` | QuickJS opcode(s) | Notes |
|---|---|---|---|
| `dup` | `qjs_val_dup` | `dup` | One input, two outputs — QuickJS's `dup` is a refcount bump, not a deep copy. |
| `drop` | `qjs_val_drop` | *(none)* | No-op at the bytecode level — QuickJS's own refcounting/GC handles release; nothing needs emitting. |
| `move` | `qjs_val_move` | *(none)* | Pure slot rename (`get_loc`+`put_loc` only). |
| `push` | `qjs_val_push` | `get_length`, `put_array_el` | `arr[arr.length] = value` — a real JS array-growing write; `.length` extends itself, no extra bookkeeping. See §7. |
| `pop` | `qjs_val_pop` | `get_length`, `sub`, `swap`, `get_array_el`, `put_field` (×2, see below) | Fetches `arr[length-1]`, then truncates via `arr.length = length-1`. See §7. |
| `join` (2-arg, structure ≠ `f...`) | `qjs_val_join` | `is_undefined`, conditional branch | The simple "pick whichever of two values isn't undefined" merge used inside a `frame_N` body — **not** the 3-arg `f_jj_j join` combinator, see §4. |
| `add` | `qjs_val_add` | `add` | |
| `sub` | `qjs_val_sub` | `sub` | |
| `mul` | `qjs_val_mul` | `mul` | |
| `div` | `qjs_val_div` | `div` | |
| `rem` | `qjs_val_rem` | `mod` | JS `%`. |
| `eq?` | `qjs_val_eq` | `eq` | |
| `ne?` | `qjs_val_ne` | `neq` | |
| `lt?` | `qjs_val_lt` | `lt` | |
| `le?` | `qjs_val_le` | `lte` | |
| `gt?` | `qjs_val_gt` | `gt` | |
| `ge?` | `qjs_val_ge` | `gte` | |
| `shl` | `qjs_val_ashl` | `shl` | |
| `shr` | `qjs_val_ashr` | `shr` | |
| `not` | `qjs_val_lnot` | `lnot` | Logical negation, for boolean control flow (`opt`/`join`'s `cmp3 = !cmp`). |
| `bnot` | `qjs_val_bnot` | `not` (bitwise complement) | JS `~`. Distinct from `not`/`qjs_val_lnot` above — see `Claude.md`'s note on the original `qjs_val_not` bug this split resolved. |
| `band` | `qjs_val_and` | `and` | |
| `bor` | `qjs_val_or` | `or` | |
| `bxor` | `qjs_val_xor` | `xor` | |
| `cons_obj` | `qjs_val_cons_obj` | `object` | Empty object. |
| `cons_arr` | `qjs_val_cons_arr` | `array_from 0` | Empty array. |
| `cons_str "text"` | `qjs_val_cons_str` | `push_atom_value` | The string is interned into the bytecode's atom table (`codegen::register_atom`) and referenced by atom index, not embedded inline — see §7. |
| `cons_N` / `cons_true` / `cons_false` | `qjs_val_cons_<suffix>` (auto-registered, see §2) | `push_i32 N` / `push_true` / `push_false` | |
| `get` | `qjs_val_get` | `get_array_el` | `obj × key → value` — works for both array-index and string-property keys (same opcode, dispatches on the value's own runtime tag), consuming `obj`. |
| `set` | `qjs_val_set` | `dup`, `put_array_el` | `obj × key × value → obj`. Not a single opcode: `put_array_el` consumes its object and returns nothing, but JS's `obj[k]=v` evaluates to `v` not `obj` — `set` dups the object first so a copy survives to become the result. |
| `assert` | `qjs_val_assert` | conditional `throw` | Throws (uncaught — `bcrun` reports it and the process exits nonzero) if the value is falsy. This is the entire test-verification mechanism used throughout `test/ct2qjs/`. |

Operations declared in `prelude.ct`'s signatures but with **no working
codegen case** as of this writing: `qjs_val_bot`/`qjs_val_top` (the
lattice signature's other two ops — never referenced by any `.ct` file in
this repo), and `qjs_val_opt` (bound in `builtins.ct`, but dead: `opt` is
*always* intercepted as the `fn_opt` control-flow case by `classify()`
before the builtins table is ever consulted, so this binding can never be
reached — see §4). `qjs_val_copy` has no binding in `builtins.ct` **and**
no `codegen.cpp` case at all, despite `js2ct` already emitting `jsvalue
copy` for JS-level reassignment — see `Claude.md`'s known-bugs list.

## 7. Two multi-instruction builtins worth reading closely

Most rows in §6 are one opcode. Two aren't, and both are worth
understanding as *why*, not just *what*:

**`set` and `push`** need to hand back a reference to the object they
just mutated, but the QuickJS opcode doing the actual mutation
(`put_array_el`) consumes its object operand and pushes nothing — because
that's exactly what JS assignment expressions do (`obj[k] = v` evaluates
to `v`). Both work around this the same way: `dup`/re-`get_loc` the
object *before* the mutating write, so a second reference survives to
become the instruction's own output.

**`pop`** additionally needs to *shrink* the array, and that specifically
requires QuickJS's real built-in `JS_ATOM_length` atom — not a
`register_atom()`'d custom atom that merely prints as `"length"`.
`JS_SetPropertyInternal` (`quickjs.c`) special-cases a property write
whose atom *is* `JS_ATOM_length` on an array-class object to route
through `set_array_length()`, which is what actually frees elements past
the new length; a custom lookalike atom would just become an ordinary own
property named "length" and never truncate anything. `codegen.hpp`
hardcodes `js_atom_length = 50` (the 50th `DEF()` entry in the
currently-vendored `quickjs-atom.h`) next to the equivalent, already-
established `js_atom_end` constant — both share the same fragility
tradeoff: `cmake/quickjs.cmake` pins QuickJS to `GIT_TAG master`, not a
fixed commit, so either constant goes stale if upstream's built-in atom
table ever changes shape.
