#pragma once

#include <cstdint>
#include <stdexcept>
#include <string_view>

#include "instruction.hpp"
#include "opcodes.hpp"

namespace qthu::as
{

struct emitted_instruction
{
    instruction instr;

    operator const instruction&() const
    {
        return instr;
    }

    operator instruction&&() &&
    {
        return std::move( instr );
    }
};

inline emitted_instruction make( std::string_view mnemonic )
{
    auto info = get_opcode_info( std::string( mnemonic ) );
    if ( !info )
        throw std::runtime_error( "unknown opcode: " + std::string( mnemonic ) );
    if ( info->size != 1 )
        throw std::runtime_error( "opcode requires operand: " + std::string( mnemonic ) );

    return { instruction{ .mnemonic = std::string( mnemonic ),
                                .opcode = info->opcode,
                                .size = info->size } };
}

inline emitted_instruction make( std::string_view mnemonic, int32_t operand )
{
    auto info = get_opcode_info( std::string( mnemonic ) );
    if ( !info )
        throw std::runtime_error( "unknown opcode: " + std::string( mnemonic ) );
    if ( info->size == 1 )
        throw std::runtime_error( "opcode does not take operand: " + std::string( mnemonic ) );

    return { instruction{ .mnemonic = std::string( mnemonic ),
                                .opcode = info->opcode,
                                .size = info->size,
                                .operand = opr( static_cast< uint32_t >( operand ) ) } };
}

inline emitted_instruction make_label( std::string_view mnemonic, std::string_view label )
{
    auto info = get_opcode_info( std::string( mnemonic ) );
    if ( !info )
        throw std::runtime_error( "unknown opcode: " + std::string( mnemonic ) );
    if ( info->size == 1 )
        throw std::runtime_error( "opcode does not take operand: " + std::string( mnemonic ) );

    return { instruction{ .mnemonic = std::string( mnemonic ),
                                .opcode = info->opcode,
                                .size = info->size,
                                .address = addr( label ),
                                .has_address = true } };
}

#define SHORT_OPCODES 1
#define DEF( id, size, n_pop, n_push, f )                                                    \
    inline emitted_instruction id##_()                                                       \
    {                                                                                        \
        return make( #id );                                                                  \
    }                                                                                        \
    inline emitted_instruction id##_( int32_t operand )                                      \
    {                                                                                        \
        return make( #id, operand );                                                         \
    }                                                                                        \
    inline emitted_instruction id##_( std::string_view label )                               \
    {                                                                                        \
        return make_label( #id, label );                                                     \
    }
#define def( id, size, n_pop, n_push, f )

#include "quickjs-opcode.h"

#undef SHORT_OPCODES
#undef DEF
#undef def

inline emitted_instruction jz( std::string_view label )
{
    return if_false_( label );
}

inline emitted_instruction jnz( std::string_view label )
{
    return if_true_( label );
}

}
