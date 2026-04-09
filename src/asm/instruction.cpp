#include <stdexcept>
#include <format>

#include "instruction.hpp"
#include "opcodes.hpp"
#include "../common/lexer_base.hpp"

namespace qthu::as
{

instruction instruction::from_string( const std::string_view sv )
{
    lexer_base lex( sv );
    lex.drop_blanks();

    auto mnemonic_sv = lex.shift_word();
    if ( mnemonic_sv.empty() )
        throw std::runtime_error( "empty instruction" );

    std::string mnemonic( mnemonic_sv );

    if ( !is_valid_instruction( mnemonic ) )
        throw std::runtime_error( std::format( "unknown instruction: {}", mnemonic ) );

    const auto& info = get_opcode_info( mnemonic );
    if ( !info )
        throw std::runtime_error( std::format( "cannot get info for: {}", mnemonic ) );

    instruction instr( mnemonic, info->opcode, info->size );
    if ( info->size == 1 )
        return instr; // no operands

    lex.drop_blanks();
    if ( lex.sv.empty() )
        throw std::runtime_error( std::format( "missing operand for: {}", mnemonic ) );

    if ( isalpha( lex.sv[ 0 ] ) )
    {
        auto label = lex.shift_word_with( '_' );
        instr.address = addr( label );
        instr.has_address = true;
    }
    else
    {
        long value = lex.shift_signed();
        instr.operand = opr( static_cast< uint32_t >( value ) );
    }

    return instr;
}

} // namespace qthu
