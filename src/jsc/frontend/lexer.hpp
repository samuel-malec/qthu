#pragma once

#include <cassert>
#include <charconv>
#include <string>
#include <set>
#include <memory>

#include "token.hpp"

namespace jsc 
{

static const std::set< std::string_view > keywords = {
    "if", "else", "for", "do", "while", "break",
    "continue", "case", "return", "function",
    "let", "const", "var", "true", "false",
};

static const std::set< std::string_view > punct = {
    "(", ")", "{", "}", "[", "]",
    ";", ",", ".", "?", ":", "+", "-", "*", "/", 
    "%", "|", "&", "^", "~", "=", "!", "<", ">",
    "++", "--", "&&", "||", "==", "!=", "<=", ">=",
    "<<", ">>", "->", "+=", "-=", "*=", "/=", "%=",
    "|=", "&=", "^=", "<<=", ">>=", "==="
};

struct lexer
{
    using sv_t = std::string_view;
    using cat = token::cat_t;

    location loc;
    sv_t sv;
    token_sink& out;
    int ptr = 0;

    lexer( source_ptr doc, token_sink& out ) : 
        loc{ doc },
        sv{ doc->data },
        out{ out }
    {}

    void lex()
    {
        while ( !empty() )
        {
            next();
            advance();
        }
    }

    void next();

    bool empty() const { return ptr == sv.size(); }

    void push( cat c ) { out.push( token{ loc, sv.substr( 0, ptr ), c } ); }

    void advance( int offset = 0 )
    {
        assert( ptr + offset <= sv.size() );
        for ( auto c : sv.substr( 0, ptr + offset ) )
        {
            if ( c == '\n' )
            {
                loc.line++;
                loc.col = 1;
            }
            else
                loc.col++;
        }
        sv.remove_prefix( ptr );
        ptr = 0;
    }

    void drop_blanks()
    {
        while ( ptr < sv.size() && isspace( sv[ ptr ] ) )
            ++ptr;
        advance();
    }

    bool try_drop( sv_t str )
    {
        if ( sv.starts_with( str ) )
        {
            advance( str.size() );
            return true;
        }
        return false;
    }

    std::string_view shift_word() { return shift_word_with( '_' ); }
    
    std::string_view shift_word_with( auto... extra_allowed )
    {
        if ( sv.empty() )
            return sv;
        
        auto it = sv.begin();
        while ( isalpha( *it ) || ( ( *it == extra_allowed ) || ... ) )
            while ( isalnum( *it ) || ( ( *it == extra_allowed ) || ... ) )
                ++it;
        
        int size = it - sv.begin();
        ptr += size;
        auto b = sv.substr( 0, ptr );
        return b;
    }

    bool try_unsigned()
    {
        if ( sv.empty() )
            return false;

        uint64_t res{};
        auto [ xptr, ec ] = std::from_chars( sv.data(), sv.data() + sv.size(), res );
        if ( ec != std::errc() )
            return false;
        
        int size = xptr - sv.begin();
        ptr += size;
        return true;
    }
};

}
