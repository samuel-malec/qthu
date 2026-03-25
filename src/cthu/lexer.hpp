#pragma once

#include <cassert>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "../common/utf8.hpp"

namespace cthu
{
    struct source_file 
    {
        std::string name, data;

        source_file( std::string n, std::string d )
            : name{ std::move( n ) },
              data{ std::move( d ) }
        {}
    };

    using source_ptr = std::shared_ptr< source_file >;

    struct location
    {
        source_ptr doc;
        int line = 0, col = 0, byte = 0;
    };

    struct token
    {
        enum cat_t { invalid, comment,
                     lambda, kw_struct, kw_sig, kw_type, kw, eol, punct,
                     ident, arrow, bracket, paren } cat = invalid;

        location loc;
        std::string_view data;
    };

    auto &operator<<( auto &stream, location loc )
    {
        return stream << loc.doc->name << ":" << loc.line << ":" << loc.col;
    }

    auto &operator<<( auto &stream, token::cat_t cat )
    {
        switch ( cat )
        {
            case token::invalid:     return stream << "invalid";
            case token::comment:     return stream << "comment";
            case token::lambda:      return stream << "lambda";
            case token::kw_type:     return stream << "type";
            case token::kw_struct:   return stream << "struct";
            case token::kw_sig:      return stream << "sig";
            case token::kw:          return stream << "kw";
            case token::eol:         return stream << "eol";
            case token::punct:       return stream << "punct";
            case token::ident:       return stream << "ident";
            case token::arrow:       return stream << "arrow";
            case token::bracket:     return stream << "bracket";
            case token::paren:       return stream << "paren";
        }
        return stream << "unknown";
    }

    auto &operator<<( auto &stream, const token &tok )
    {
        return stream << "(" << tok.cat << ") " << tok.data;
    }

    struct token_sink
    {
        virtual void push( token t ) = 0;
    };

    struct token_vec : token_sink
    {
        std::vector< token > toks;

        void push( token t ) override
        {
            toks.push_back( t );
        }
    };

    struct lexer
    {
        using sv_t = std::string_view;
        using sv32_t = std::u32string_view;
        using cat = token::cat_t;

        source_ptr doc;
        sv_t data;
        token_sink &out;
        location loc;
        cat cnow = token::invalid;
        int ptr = 0;

        char32_t peek() const
        {
            assert( ptr < data.size() );
            auto [ ch, rem ] = decode_utf8( data.substr( ptr ) );
            return ch;
        }

        void shift( int n = 1 )
        {
            auto all = data.substr( ptr );
            auto [ ch, rem ] = decode_utf8( all );
            ptr += all.size() - rem.size();

            if ( n > 1 )
                return shift( n - 1 );
        }

        bool empty() const { return ptr == data.size(); }
        void start( cat c ) { cnow = c; }

        bool start( sv32_t s, cat c )
        {
            if ( !peek( s ) )
                return false;
            start( c );
            shift( s.size() );
            return true;
        }

        void drop()
        {
            for ( auto c : data.substr( 0, ptr ) )
                if ( c == '\n' )
                {
                    loc.line ++;
                    loc.col = 0;
                }
                else
                    loc.col ++;

            data.remove_prefix( ptr );
            loc.byte += ptr;
            ptr = 0;
        }

        bool peek( sv32_t s ) const
        {
            auto all = data.substr( ptr );

            for ( auto expect : s )
            {
                if ( all.empty() )
                    return false;
                auto [ ch, rem ] = decode_utf8( all );
                if ( ch != expect )
                    return false;
                all = rem;
            }

            return true;
        }

        bool accept( sv32_t s, cat c )
        {
            if ( !peek( s ) )
                return false;
            shift( s.size() );
            push( c );
            return true;
        }

        bool accept( sv32_t s, cat c, bool &b )
        {
            return b = accept( s, c );
        }

        bool skip( sv32_t s, bool &b )
        {
            if ( skip( s ) )
                return b = true;
            else
                return false;
        }

        bool skip( sv32_t s )
        {
            if ( peek( s ) )
                return shift( s.size() ), drop(), true;
            else
                return false;
        }

        bool peek_any( sv32_t s ) const
        {
            return s.find( peek() ) != s.npos;
        }

        bool accept_any( sv32_t s, cat c )
        {
            if ( peek_any( s ) )
                return shift(), push( c ), true;
            else
                return false;
        }

        sv_t token_data() const
        {
            return { data.begin(), size_t( ptr ) };
        }

        void push( cat c )
        {
            if ( ptr )
            {
                out.push( token{ c, loc, token_data() } );
                drop();
            }

            cnow = cat::invalid;
        }

        void push() { push( cnow ); }

        lexer( source_ptr d, token_sink &out )
            : doc{ d }, data{ d->data }, out{ out }
        {
            loc.doc = doc;
            loc.line = 1;
        }

        bool compatible( cat c, char32_t ch );
        void next();
    };

    inline std::vector< token > lex( source_ptr d )
    {
        token_vec tokens{};
        lexer l{ d, tokens };
        while ( !l.empty() )
            l.next();
        return tokens.toks;
    }
}
