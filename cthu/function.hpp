#pragma once
#include "opcode.hpp"

/* A function is a parametrized sequence of instructions where the parameters
 * equate to instruction operands (and since operands are stacks, so are
 * function parameters). Functions are always pure (i.e. cthu is a purely
 * functional language) in the sense that the outputs only depend on the
 * inputs.
 *
 * Functions are first-class values; to invoke a function, it has to be pushed
 * onto a stack and a ‹call› operation in an appropriate structure will
 * evaluate it. Such evaluation is always equivalent to taking the definition,
 * replacing formal parameters with actual parameters, and splicing the result
 * into the caller. The only actual difference is that the program obtained
 * through the latter process may fail to be finite since recursion is
 * permitted.
 *
 * Functions are not permitted to alter any non-parameter stacks (in the
 * sense that after the function ends, all non-parameter stacks must be in
 * the exact same state as they were just before it started). However, since
 * local (temporary) stacks must be empty by the time the function ends, the
 * machine is free to share all non-parameter stacks arbitrarily between
 * function invocations.
 *
 * This means that, to inline or invoke a subroutine, for each parameter, the
 * formal/actual indices can be simply swapped. Consider this situation (the
 * usual symbolic operands are replaced with actual indices here):
 *
 *     structure main
 *     (
 *         roundabout_add_1 = λ 0 → 1
 *         (
 *             i₃₂ const_1     → 7
 *             i₃₂ add     0 7 → 8
 *             w₃₂ move    8   → 1
 *         )
 *     
 *         run = λ 0 → 1
 *         (
 *             main roundabout_add_1 → f
 *             fᵢⁱ  call f 7         → 8
 *         )
 *     )
 *
 * The call can be performed by swapping 0↔7 and 1↔8 to obtain:
 *
 *     i₃₂ const_1     → 0
 *     i₃₂ add     7 0 → 1
 *     w₃₂ move    1   → 8
 *
 * Since there is no overall effect on stacks 0 and 1 (as seen from the caller)
 * there is no concern over capture and none of the usual countermeasures need
 * to be deployed. */

namespace cthu
{
    struct function_t
    {
        std::vector< insn > body;
    };

    using function_ptr = function_t *;
}
