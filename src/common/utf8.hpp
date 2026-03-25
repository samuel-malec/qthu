#pragma once

#include <string_view>
#include <cstdint>

static std::pair< char32_t, std::string_view >
decode_utf8( std::string_view f, bool report_partial = true )
{
    auto get = [&]( int i ) -> uint8_t { return f[ i ]; };
    uint32_t a = get( 0 ), b, c, d;
    auto error = std::make_pair( a | 0xdc80, f.substr( 1 ) );
    auto partial = std::make_pair( 0xffffu, f );

    if ( a < 0x80 )
        return { a, f.substr( 1 ) };

    if ( a < 0xC2 )
        return error;

    if ( a < 0xE0 )
    {
        if ( f.size() < 2 )
            return report_partial ? partial : error;

        b = get( 1 );

        if ( ( b & 0xC0 ) != 0x80 )
            return error;

        return { ( ( a & 0x1f ) << 6 ) | ( b & 0x3f ), f.substr( 2 ) };
    }

    if ( a < 0xF0 )
    {
        if ( f.size() < 3 )
            return report_partial ? partial : error;

        b = get( 1 ), c = get( 2 );

        switch ( a )
        {
            case 0xE0: if ( ( b & 0xe0 ) != 0xa0 ) return error; else break;
            case 0xED: if ( ( b & 0xe0 ) != 0x80 ) return error; else break;
            default:   if ( ( b & 0xc0 ) != 0x80 ) return error; else break;
        }

        if ( ( c & 0xc0 ) != 0x80 )
            return error;

        return { ( ( a & 0x0f ) << 12 ) | ( ( b & 0x3f ) << 6 ) | ( c & 0x3f ), f.substr( 3 ) };
    }

    if ( a < 0xF5 )
    {
        if ( f.size() < 4 )
            return report_partial ? partial : error;

        b = get( 1 ), c = get( 2 ), d = get( 3 );

        switch ( a )
        {
            case 0xF0: if ( b < 0x90 || b > 0xBF ) return error; else break;
            case 0xF4: if ( ( b & 0xf0 ) != 0x80 ) return error; else break;
            default:   if ( ( b & 0xc0 ) != 0x80 ) return error; else break;
        }

        if ( ( c & 0xc0 ) != 0x80 || ( d  & 0xc0 ) != 0x80 )
            return error;

        return { ( ( a & 0x07 ) << 18 ) | ( ( b & 0x3F ) << 12 ) |
                    ( ( c & 0x3F ) << 6 )  | ( d & 0x3F ), f.substr( 4 ) };
    }

    return error;
}
