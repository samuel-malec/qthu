#include <stdexcept>
#include <format>

#include "directive.hpp"
#include "../common/lexer_base.hpp"

namespace qthu
{

const std::set< std::string_view > directive::valid_directives = { "function", "args", "locals",
                                                                   "stack_size" };

directive directive::from_string( const std::string_view sv )
{
    lexer_base lex( sv );
    lex.drop_blanks();

    auto mnemonic_sv = lex.shift_word();
    if ( mnemonic_sv.empty() )
        throw std::runtime_error( "empty directive" );

    if ( valid_directives.find( mnemonic_sv ) == valid_directives.end() )
        throw std::runtime_error(
            std::format( "invalid directive: '{}'", std::string( mnemonic_sv ) ) );

    auto mnemonic = std::string( mnemonic_sv );

    lex.drop_blanks();
    if ( lex.sv.empty() )
        throw std::runtime_error( std::format( "missing operand for: {}", mnemonic ) );

    directive dir;
    dir.mnemonic = std::string( mnemonic_sv );
    bool is_function = mnemonic_sv == "function";

    if ( is_function && !isalpha( lex.sv[ 0 ] ) )
        throw std::runtime_error(
            std::format( "invalid function name: '{}'", std::string( lex.sv ) ) );

    if ( !is_function && !isdigit( lex.sv[ 0 ] ) )
        throw std::runtime_error(
            std::format( "invalid operand for directive: '{}', got: '{}', but expected a number",
                         mnemonic, std::string( lex.sv ) ) );

    if ( isalpha( lex.sv[ 0 ] ) )
    {
        auto name = lex.shift_word();
        dir.name = name;
    }
    else
    {
        long value = lex.shift_signed();
        dir.value = static_cast< uint32_t >( value );
    }

    return dir;
}

} // namespace qthu