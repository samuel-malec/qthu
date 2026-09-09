# JS → Cthulhu dialect, v0.1

Conventions for lowering the current `js2ct` AST subset (block, var_declaration,
fn_declaration, if, while, do-while, for, return, break, continue, expr_stmt /
literals, var, unary, binary, assign, call) into `cthu` structures, derived by
hand from the existing `fib.ct` idiom rather than re-deriving control flow from
scratch. cthuc performs no type/linearity checking today (no checker exists),
so these are *design* conventions, not enforced constraints yet.

## Conventions

1. **Every JS function becomes its own `structure`**, named after the function.
2. **A function's entry lambda is always named `run`**, params as `in`, one `out`.
3. **The top-level script is `structure main`, entry `run`, no output** — this
   matches `cthuc`'s hardcoded `find_main_id()` lookup (structure `main`,
   function `run`). Side effects / `assert` only, no return value.
4. **Tail-position `if`/`else`** (nothing follows it in its block) lowers
   exactly like `fib.ct`'s `base`/`rec`/`frame`/`loop`: `then_N`/`else_N`
   closures + a `frame_N` combinator + `opt`/`join`/`call`.
5. **Non-tail-position `if`/`else`** (code follows it): hoist everything
   *after* the conditional into its own closure `cont_N`, taking the full
   live scope as parameters. Each branch computes its values and tail-calls
   `cont_N` instead of falling through. See example 3.
6. **Loops are self-recursion.** A `while`/`do-while` becomes a `loop_N`
   lambda that references *itself by name* (`struct_name loop_N → self`)
   through the same `opt`/`join`/`call` dispatch `fib.ct` uses for `rec`.
   This works today with zero changes to `cthuc`: function-ref resolution
   (`ir.hpp::classify`, the `fn_ref` case) just looks a name up in the
   symbol table, which is fully populated before any lambda body is
   resolved — a lambda can reference any sibling, including itself.
   See example 4.
7. **Calls to named JS functions**: `struct_name run → f_ref` then
   `f_<n>i_i call f_ref args… → out`, where the call-signature "structure"
   name (`f__i`, `f_i_i`, `f_ii_i`, `f_iii_i`, …) is a **pure naming
   convention** — see the discovery note below — matching exactly what
   `structure_builder::call_signature_name` in `linear2cthu.hpp` already
   generates.
8. **Every closure that's a target of `opt`/`join`/`call` takes the same
   input tuple** (the full set of live bindings at that point) and produces
   one output; a branch that doesn't need some input just `drop`s it —
   exactly `fib.ct`'s `base` (drops `a` and `i`, keeps only `b`).

## Mechanical discovery: call-arity "structures" aren't declared anywhere

`ir.hpp::classify()` recognizes `call`/`opt`/`join` **by operation name
alone** (`join` additionally requires the structure name to start with
`"f"`), regardless of whether that structure was ever declared with
`structure ... ( ... )`. `fib.ct` already relies on this: it uses
`fᵢᵢᵢⁱ call/opt/join` throughout, but no `structure fᵢᵢᵢⁱ` exists in
`builtins.ct`. So call-signature names need **no** `builtins.ct` entries —
only a consistent naming convention. I switched from `fib.ct`'s unicode
names (`fᵢⁱ`, `fᵢᵢᵢⁱ`) to the ASCII pattern `js2ct` already generates
(`f_i_i`, `f_iii_i`, `f__i` for 0-ary) so the hand-written examples below
match generated output exactly.

## Fixed bug (was carried forward here, now resolved in `cthuc`)

`codegen.cpp`'s `qjs_val_not` case w~as missing `put_loc_` for its output and
emitted the *bitwise* `not_()` opcode, but every `if`/loop example below
depends on it for *boolean* negation (`jsvalue not cmp₂ → cmp₃`, used to
build the "else" half of `opt`/`join`). Split into two distinct builtins: a
correctly-implemented `qjs_val_lnot` (logical negation, via the `lnot_()`
opcode, for control flow — used as `not` below) and a separate
`qjs_val_bnot` (bitwise complement, for JS `~`, declared in `builtins.ct`
but not yet wired to a working codegen case).

## Grammar coverage

These examples only exercise: function declarations, `let`, reassignment,
literals, binary arithmetic/comparison, `if`/`else`, `while`, `return`,
direct calls. Not yet covered: `for`, `do-while`, `break`/`continue` inside
loops, `&&`/`||` (needs the same hoisting as example 3, flagged above but
not derived here), closures capturing outer bindings, multi-arg mutual
recursion. Suggested as the next batch once these four are wired up.