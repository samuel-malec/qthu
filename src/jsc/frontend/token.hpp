#pragma once

#include <string>
#include <string_view>
#include <memory>

namespace jsc
{

struct source_file
{
    std::string name, data;
        
    source_file( std::string name, std::string data ) :
            name{ std::move( name ) },
            data{ std::move( data ) }
    {}
};

using source_ptr = std::shared_ptr< source_file >;

struct location
{
    source_ptr doc;
    int line = 1, col = 1, byte = 0;
};

inline std::ostream& operator<<( std::ostream& os, const location& loc )
{
    os << "Ln " << loc.line << ", Col " << loc.col;
    return os;
}

struct token
{
    location loc;
    std::string_view data;
    enum cat_t
    {
        invalid,
        punct,
        keyword,
        ident,
        number,
    } cat = invalid;
};

inline std::ostream& operator<<( std::ostream& os, const token::cat_t c )
{
    switch ( c )
    {
        case token::punct:    return os << "punct";
        case token::keyword:  return os << "keyword";
        case token::ident:    return os << "ident";
        case token::number:   return os << "number";
        case token::invalid:  return os << "invalid";
    }
    return os << "unknown";
}

inline std::ostream& operator<<( std::ostream& os, const token& t )
{
    os << t.loc << "[ " << t.data << ", " << t.cat << " ]";
    return os;
}

struct token_sink
{
    virtual void push( token tok ) = 0;
};

}
