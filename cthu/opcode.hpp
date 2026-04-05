#pragma once

/* Instructions are encoded in two parts:
 *
 *  1. The operation, encoded in 28 bits, which can be in one of 3 forms:
 *
 *     a. «structured opcodes» under 0x100'0000 give a structure id and an
 *        operation id within that structure (16 + 8 bits),
 *     b. «subroutine opcodes» above 0x100'0000 but below 0xeff'0000 (each
 *        corresponds to a single user-defined subroutine), and finally
 *     c. «physical opcodes» above 0xeff'0000 but below 0xf00'0000 directly
 *        encode low-level machine operations.
 *
 *  2. Each operand is encoded in 9 bits, allowing for 4 'inline' operands to
 *     be part of the opcode. Operands are given as the id of the stack to be
 *     used for this operand. The meaning and direction of individual operands
 *     is opcode-specific. Unused operands use value 511. Stacks are 0-indexed.
 *     All valid operands must be distinct (i.e. any given stack index must
 *     appear at most once).
 *     
 *     If further operands are needed, an extension opcode can follow, encoded
 *     as 0xf00'0000 + 2 operands, followed by the usual 4 operands, totalling
 *     6 operands per extension (i.e. once-extended opcode can carry 10
 *     operands, and twice-extended 16).
 *
 * Operand extensions unfortunately mean that the encoding is not fixed-width,
 * but at least it uses fixed-size chunks and the meaning of each chunk is
 * uniquely determined in isolation.
 *
 * Input programs can only contain structured and subroutine opcodes. As the
 * final step before execution, they are rewritten to only contain subroutine
 * and physical opcodes. */

namespace cthu
{
    /* An ‹opcode› is implemented as an instruction with all operands set to
     * -1. This prevents a bare opcode from being accidentally executed. */

    struct opcode
    {
        uint64_t enc;

        bool is_physical()   const { return ( enc & 0xfff'0000 ) == 0xfff'0000; }
        bool is_structured() const { return ( enc & 0xf00'0000 ) == 0x000'0000; }
        bool is_subroutine() const { return !is_physical() & !is_structured(); }

        uint32_t get_physical() const
        {
            ASSERT( is_physical() );
            return enc & 0xffff;
        }

        uint32_t get_structure() const
        {
            ASSERT( is_structured() );
            return enc & 0xffff;
        }

        uint32_t get_structure_op() const
        {
            ASSERT( is_structured() );
            return ( enc >> 16 ) & 0xff;
        }

        uint32_t get_subroutine() const
        {
            ASSERT( is_subroutine() );
            return ( enc & 0xfff'ffff ) - 0x100'0000;
        }
    };

    struct insn : opcode
    {
        uint32_t operand( int id ) const
        {
            ASSERT_LEQ( 0, id );
            ASSERT_LEQ( id, 4 );

            return ( enc >> ( 28 + id * 9 ) ) & 0x1ff;
        }
    };
}
