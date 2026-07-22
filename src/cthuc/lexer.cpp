#include <format>

#include "lexer.hpp"

namespace qthu::cthuc
{
    bool lexer::compatible( cat c, char32_t ch )
    {
        if ( c == cat::comment )
            return ch != '\n';
        if ( c == cat::ident )
            return ch <= 255 && std::isalnum( ch ) ||
                   ch >= 0x2070 && ch < 0x20a0 ||
                   ch >= 0x00b0 && ch < 0x00c0 ||
                   ch >= 0x1d62 && ch < 0x1d66 ||
                   ch == U'?' || ch == U'_' || ch == U'\'';

        return false;
    }

    void lexer::next()
    {
        if ( compatible( cnow, peek() ) )
            return shift();
        else
        {
            if ( cnow == cat::ident )
            {
                if ( token_data() == "type" )
                    cnow = cat::kw_type;
                if ( token_data() == "structure" )
                    cnow = cat::kw_struct;
                if ( token_data() == "signature" )
                    cnow = cat::kw_sig;
            }

            if ( cnow != cat::invalid )
                return push();
        }

        if ( peek_any( U"\t\r " ) )
            return shift(), drop();

        if ( auto c = peek(); c <= 255 && std::isalpha( c ) )
            return start( cat::ident );

        if ( accept_any( U"[]", cat::bracket ) )
            return;

        if ( accept_any( U"()", cat::paren ) )
            return;

        if ( accept( U"\n", cat::eol ) )
            return;

        if ( accept( U":", cat::punct ) ||
             accept( U"∷", cat::punct ) ||
             accept( U"×", cat::punct ) ||
             accept( U"=", cat::punct ) ||
             accept( U",", cat::punct ) )
            return;

        if ( start( U";", cat::comment ) )
            return;

        if ( accept( U"λ", cat::lambda ) ||
             accept( U"∅", cat::ident ) ||
             accept( U"→", cat::arrow ) ||
             accept( U"->", cat::arrow ) )
        {
            return;
        }
        
        throw std::runtime_error( std::format( "Invalid .ct format at line: {}, in column: {}", loc.line, loc.col ) );
    }
}
