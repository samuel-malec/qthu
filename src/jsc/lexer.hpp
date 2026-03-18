#pragma once

#include <string_view>
#include <string>
#include <vector>
#include <set>
#include <stdexcept>
#include <format>

#include "token.hpp"
#include "../common/lexer_base.hpp"
#include "../common/limits.hpp"

namespace jsc
{

static const std::set< std::string_view > punct = {
    ";",  ",",  ".",  "?",  ":",  "(",  ")",  "{",  "}",  "+",  "-",  "*",   "/",   "%",  "|",
    "&",  "^",  "~",  "=",  "!",  "<",  ">",  "++", "--", "&&", "||", "==",  "!=",  "<=", ">=",
    "<<", ">>", "->", "+=", "-=", "*=", "/=", "%=", "|=", "&=", "^=", "<<=", ">>=",
};

static const std::set< std::string_view > keywords = { "if", "else", "for", "return", "function" };

std::string print( const std::vector< token >& tokens )
{
    std::string result;
    for ( const auto& token : tokens )
        result += "cat: " + std::string( to_string( token.cat ) ) + " lit: [" + std::string( token.lit ) + "]\n";
    return result;
}

std::vector< token > lex( std::string_view data )
{
    using cat = token::category;

    qthu::lexer_base lex{ data };
    auto& sv = lex.sv;
    std::vector< token > out{};

    while ( lex.drop_blanks(), lex )
    {
        auto begin = sv.data();
        // comments
        if ( lex.try_drop( "//" ) )
        {
            auto nl = sv.find_first_of( '\n' );
            if ( nl == sv.npos )
                throw std::runtime_error( "No newline to end single-line comment" );
            nl++;
            sv.remove_prefix( nl );
            continue;
        }

        // identifiers and keywords
        if ( auto w = lex.shift_word(); !w.empty() )
        {
            out.emplace_back( w, keywords.contains( w ) ? cat::keyword : cat::ident );
            continue;
        }

        // number
        if ( auto l = lex.try_unsigned() )
        {
            auto lit = std::string_view( begin, sv.data() - begin );
            if ( !qthu::within_i32( l.value() ) )
                throw std::runtime_error( "Integer literal too large" );

            out.emplace_back( lit, cat::number, l.value() );
            continue;
        }

        // punctiation
        bool seen_punct = false;
        for ( int len : { 3, 2, 1 } )
        {
            if ( sv.size() < static_cast< std::size_t >( len ) )
                continue;

            auto substr = sv.substr( 0, len );
            if ( punct.contains( substr ) )
            {
                sv.remove_prefix( len );
                out.emplace_back( substr, cat::punct );
                seen_punct = true;
                break;
                
            }
        }

        if ( seen_punct )
            continue;
        throw std::runtime_error( std::format( "Unexpected character: '{}'", sv[ 0 ] ) );
    }

    return out;
}

}
