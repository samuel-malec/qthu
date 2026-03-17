#pragma once

#include <string_view>
#include <string>
#include <vector>
#include <set>

namespace jsc
{

static const std::set< std::string_view > punct = {
    ";",  ",",  ".",  "?",  ":",  "(",  ")",  "{",  "}",  "+",  "-",  "*",   "/",   "%",  "|",
    "&",  "^",  "~",  "=",  "!",  "<",  ">",  "++", "--", "&&", "||", "==",  "!=",  "<=", ">=",
    "<<", ">>", "->", "+=", "-=", "*=", "/=", "%=", "|=", "&=", "^=", "<<=", ">>=",
};

struct location
{
    int line;
    int col;
};

enum class category
{
    unknown = 0,
    keyword,
    punct,
    ident,
    number,
};

struct token
{
    category cat;
    location loc;
    std::string lexeme;
};

struct lexer
{
    std::string_view data;
    std::vector< token > tokens;
    location loc{ 1, 0 };
    int ptr = 0;

    bool empty() const
    {
        return data.empty();
    }

    void next()
    {
        char curr = peek();
    }

    void drop()
    {
        for ( int i = 0; i < ptr; ++i )
        {
            if ( data[ i ] == '\n' )
            {
                loc.line++;
                loc.col = 0;
            }
            else
                loc.col++;
        }

        data.remove_prefix( ptr );
        ptr = 0;
    }

    void push( category c )
    {
        std::string lexeme = std::string( data.substr( 0, ptr ) );
        tokens.push_back( { c, loc, lexeme } );
        drop();
    }

    char peek() const
    {
        if ( empty() )
            return '\0';
        return data[ 0 ];
    }
};

std::vector< token > lex( std::string_view data )
{
    lexer lex{ data };

    while ( !lex.empty() )
        lex.next();

    return std::move( lex.tokens );
}

} // namespace jsc
