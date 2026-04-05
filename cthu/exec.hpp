#pragma once
#include "state.hpp"
#include "program.hpp"

/* Cthu programs are linear sequences of instructions and control flow is quite
 * restricted. There is no arbitrary jump instruction, much less conditional or
 * even indirect jumps. The reason for this is simplicity of program rewriting
 * – keeping track of jump targets would make rewriting much more complicated
 * and error-prone.
 *
 * What we have instead is strictly nested, non-deterministic (and hence
 * unconditional) control flow. To this effect, there are 4 builtin
 * instructions: fork and join, both in a left and a right variant. Forks cause
 * execution to ‘split’ into two while joins allow these executions to merge
 * again.
 *
 * Forks and joins must be paired (this is what strictly nested above means):
 *
 *  • a left fork with a right join, or
 *  • a left join with a right fork.
 *
 * The effects are as follows:
 *
 *  • a left fork either:
 *    a. continues execution as if nothing happened, or
 *    b. skips ahead to its matching right join,
 *  • a right fork either:
 *    a. continues execution as if nothing happened, or
 *    b. replays the instruction sequence starting with its matching left join.
 *
 * Let us call the (a) cases passive and the (b) cases active. Then, since
 * programs are linear, an active left fork can be understood to simply ignore
 * all instructions until its matching join is found. On the flip side, an
 * active left «join» causes instructions to be executed as normal, but also
 * «recorded» so that the corresponding (active) fork may re-execute them. Of
 * course, these effects can be nested.
 *
 * Notably, subroutine opcodes can be skipped or recorded as-is (most
 * importantly, they do not need to be unfolded for recording, though they do
 * need to be executed as usual).
 *
 * Finally, since programs are generally immutable once executing, an efficient
 * implementation does not need to actually examine the instructions between a
 * fork and a join on every execution – the number of instructions to skip
 * forward or back can be either precomputed once the program is in its final
 * form, or cached after a pair is executed once. */

/* Additionally, a restriction is placed on program segments enclosed in
 * fork–join pairs, namely that they do not affect «stack shape»; that is, they
 * must not change the height of any stacks, nor the types of values stored on
 * them. This restriction is similar to the one placed on subroutines, although
 * it is not quite as strict. */

/* Finally, existence of forks clearly causes the computation to branch into a
 * state «tree», but computations of regular (deterministic) programs form
 * state «sequences». And ideally, we would like cthu programs to be able to
 * produce an unambiguous result.
 *
 * For this reason, we need instructions which «prune» the resulting tree,
 * eventually cutting off all branches except for one. Such instruction needs
 * to be «conditional», to recover the ability to direct control flow based on
 * data (values). To avoid confusion between assertions and pruning, the
 * pruning operation is called ‹bail›, written as ‹bail if foo› or ‹bail unless
 * foo›.
 *
 * If a ‹bail› executes in a branch, that branch is immediately abandoned (note
 * that this is different from termination). In normal execution, exactly one
 * branch must terminate – anything else is an error (i.e. if two or more
 * branches terminate, or if no branches survive). */

namespace cthu
{
    struct exec
    {
    };
}
