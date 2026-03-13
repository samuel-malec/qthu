#pragma once

#include <optional>
#include <string_view>
#include <ctype.h>

namespace qthu
{

inline int from_hex( char c )
{
    return c - ( isdigit( c ) ? '0' : ( isupper( c ) ? 'A' : 'a' ) - 10 );
}

struct lexer
{
    std::string_view sv;
    bool ok = true;

    explicit lexer( std::string_view sv ) : sv( sv ) { }

    explicit operator bool() const
    {
        return ok && !sv.empty();
    }

    void drop_blanks()
    {
        while ( !sv.empty() && isspace( sv[ 0 ] ) )
            sv.remove_prefix( 1 );
    }

    void trim_blanks()
    {
        while ( !sv.empty() && isblank( sv.back() ) )
            sv.remove_suffix( 1 );
    }

    bool drop( std::string_view str )
    {
        return ok = ok && try_drop( str );
    }

    bool try_drop( std::string_view str )
    {
        if ( sv.starts_with( str ) )
            return sv.remove_prefix( str.length() ), true;
        return false;
    }

    bool try_trim( std::string_view str )
    {
        if ( sv.ends_with( str ) )
            return sv.remove_suffix( str.length() ), true;
        return false;
    }

    std::string_view shift_label()
    {
        return shift_word_with( '.', '_' );
    }

    std::string_view shift_word()
    {
        return shift_word_with( '_' );
    }

    std::string_view shift_word_with( auto... extra_allowed )
    {
        if ( sv.empty() )
            return sv;
        auto it = sv.begin();
        if ( isalpha( *it ) || ( ( *it == extra_allowed ) || ... ) )
            while ( isalnum( *it ) || ( ( *it == extra_allowed ) || ... ) )
                ++it;
        auto size = it - sv.begin();
        auto b = sv.substr( 0, size );
        sv.remove_prefix( size );
        return b;
    }

    long shift_signed()
    {
        char* end;
        auto n = strtol( sv.data(), &end, 0 );
        if ( end == sv.data() )
            return ok = false, 0;
        sv.remove_prefix( end - sv.data() );
        return n;
    }

    std::optional< unsigned long > try_unsigned()
    {
        if ( sv.empty() || !isdigit( sv.front() ) )
            return {};
        char* end;
        auto n = strtoul( sv.data(), &end, 0 );
        if ( end == sv.data() )
            return {};
        sv.remove_prefix( end - sv.data() );
        return { n };
    }
};

} // namespace qthu
