#include <format>
#include <stdexcept>
#include <vector>

#include "lexer.hpp"

namespace qthu::jsc
{

void lexer::next()
{
    drop_blanks();
    if ( empty() )
        return;
 
    // single line comment
    if ( try_drop( "//" ) )
    {
        do
        {
            ptr = sv.find( '\n', ptr );
            if ( ptr == sv.npos )
                throw std::runtime_error("no newline after single-line comment");
            ++ptr;
        } while ( ptr > 1 && sv[ ptr - 2 ] == '\\' ||
            ptr > 2 && sv.substr( ptr - 3, 2 ) == "\\\r" );
        return;
    }

    // multi-line comment
    if ( try_drop( "/*" ) )
    {
        ptr = sv.find( "*/" );
        if ( ptr == sv.npos )
            throw std::runtime_error( "unterminated multi-line comment" );
        ptr++;
        return;
    }

    // keyword
    auto word = shift_word();
    if ( keywords.contains( word ) )
    {
        push( cat::keyword );
        return;
    }

    // identifier
    if ( !word.empty() )
    {
        push( cat::ident );
        return;
    }

    // punctuation
    for ( int l : { 3, 2, 1 } )
    {
        if ( sv.size() < l )
            continue;
        
        auto s = sv.substr( 0, l );
        if ( punct.contains( s ) )
        {
            ptr += l;
            push( cat::punct );
            return;
        }
    }

    // number
    if ( try_unsigned() )
    {
        push( cat::number );
        return;
    }
    
    throw std::runtime_error( std::format("unexpected input at file: '{}', Ln {}, Col {}", 
                              loc.doc->name, loc.line, loc.col ) );
}

}
