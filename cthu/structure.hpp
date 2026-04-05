#include "signature.hpp"
#include "opcode.hpp"
#include <map>

/* Each structure implements a single signature instance. Internally,
 * structures map from structured opcodes to either physical or subroutine
 * ones. Operand types and directions must match 1:1. Any operand remapping
 * must be handled by an additional subroutine (which then gets its own
 * subroutine opcode). */

namespace cthu
{
    struct structure_t
    {
        std::map< opcode, opcode > map;
    };

    using structure_ptr = structure_t *;
}
