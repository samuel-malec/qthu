#include "signature.hpp"
#include "opcode.hpp"
#include "function.hpp"
#include <map>
#include <vector>
#include <cstdint>

/* Each structure implements a single signature instance. Internally,
 * structures map from structured opcodes to either physical or subroutine
 * ones. Operand types and directions must match 1:1. Any operand remapping
 * must be handled by an additional subroutine (which then gets its own
 * subroutine opcode). */

namespace cthu
{
    struct sig_instance_t
    {
        uint32_t signature = 0;
        std::vector< uint32_t > args;
    };

    struct structure_t
    {
        std::map< opcode, opcode > map;
        std::vector< sig_instance_t > signatures;
        std::map< uint32_t, uint32_t > builtin_ops;
        std::map< uint32_t, function_t > functions;
    };

    using structure_ptr = structure_t *;
}
