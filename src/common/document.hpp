#pragma once

#include <algorithm>
#include <assert.h>
#include <vector>
#include <string_view>
#include <string>
#include <memory>

namespace brq
{
struct string_builder;
}

namespace qthu
{

struct location
{
    long line = 0;
    long column = 0;
    long byte = 0;
};

struct line_index
{
    std::vector< long > index;

    explicit line_index( std::string_view sv )
    {
        long last = -1;

        do
        {
            index.push_back( last );
            last = sv.find( '\n', last + 1 );
        } while ( last != sv.npos );

        if ( index.empty() || index.back() + 1 != sv.length() )
            index.push_back( sv.length() );
    }

    location translate( long byte ) const
    {
        auto i = std::lower_bound( index.begin(), index.end(), byte );
        assert( i > index.begin() ); // the first element is "line zero"
        assert( i < index.end() ); // the last element is length of the string
        auto ln = i - index.begin();
        auto col = byte - *( i - 1 );
        return { ln, col, byte };
    }

    long start_of_line( long l ) const
    {
        return index[ l - 1 ] + 1;
    }

    long end_of_line( long l ) const
    {
        return index[ l ];
    }
};

struct document : std::enable_shared_from_this< document >
{
    static constexpr std::string_view stdin_name = "<stdin>";

    const std::string filename;
    const std::string str;
    const line_index li;

    document( std::string_view f, std::string_view s )
        : filename( std::string{ f } ), str( expand_tabs( s ) ), li( str )
    {
    }

    document( const document& ) = delete;
    document( document&& ) = delete;

    bool is_stdin()
    {
        return filename == stdin_name;
    }

    std::string_view sv() const
    {
        return str;
    }

    struct span
    {
        std::shared_ptr< const document > _doc = nullptr;
        std::string_view sv;

        bool valid() const
        {
            return !!_doc;
        }

        bool zero_width() const
        {
            return sv.size() == 0;
        }

        bool only_file() const
        {
            return zero_width() && &*sv.begin() == &*doc().str.end();
        }

        explicit operator bool() const
        {
            return valid();
        }

        const document& doc() const
        {
            assert( _doc );
            return *_doc;
        }

        span last() const
        {
            assert( valid() );
            if ( zero_width() )
                return *this;
            return { _doc, sv.substr( sv.size() - 1 ) };
        }

        auto begin() const
        {
            return sv.begin();
        }

        auto end() const
        {
            return sv.end();
        }

        std::string str() const
        {
            return std::string( sv );
        }
    };

    span ref( std::string_view sub ) const
    {
        std::string_view sv{ str };
        assert( sv.begin() <= sub.begin() );
        assert( sv.begin() <= sub.end() );
        assert( sub.begin() < sv.end() );
        assert( sub.end() <= sv.end() );
        return { this->shared_from_this(), sub };
    }

    span ref_char( std::string_view s ) const
    {
        int nbytes = 1;
        while ( ( s[ nbytes ] & 0xc0 ) == 0x80 )
            ++nbytes;

        return ref( s.substr( 0, nbytes ) );
    }

    span ref_zerowidth( std::string_view s ) const
    {
        return ref( std::string_view{ s.data(), 0 } );
    }

    span ref_file() const
    {
        return { this->shared_from_this(), std::string_view( &*str.end(), 0 ) };
    }

    std::pair< int, int > view_to_indices( std::string_view sub ) const
    {
        std::string_view sv{ str };
        // The view must belong to the original string
        assert( sv.begin() <= sub.begin() );
        assert( sv.begin() < sub.end() );
        assert( sub.begin() < sv.end() );
        assert( sub.end() <= sv.end() );

        return { sub.begin() - sv.begin(), sub.end() - sv.begin() };
    }

    long pcolumn( long byte ) const
    {
        return pcolumn( sv().substr( 0, byte ) );
    }

    long pcolumn( std::string_view prefix ) const
    {
        long col = 0;
        // TODO: tabs? zero-width?
        while ( !prefix.empty() && prefix.back() != '\n' )
        {
            if ( ( prefix.back() & 0xc0 ) != 0x80 ) // ignore UTF-8 continuation bytes
                ++col;

            prefix.remove_suffix( 1 );
        }
        return col;
    }

    location translate( long byte ) const
    {
        return li.translate( byte );
    }

    std::string_view line( long l ) const
    {
        assert( l <= line_count() );

        size_t from = li.start_of_line( l ), to = li.end_of_line( l );

        return { str.c_str() + from, to - from };
    }

    long line_count() const
    {
        return li.index.size() - 1;
    }

    operator std::string_view() const
    {
        return str;
    };

    static std::string expand_tabs( std::string_view sv, int stop = 8 )
    {
        std::string s;
        s.reserve( sv.size() );

        int col = 0;
        for ( ; !sv.empty(); sv.remove_prefix( 1 ) )
        {
            if ( sv.front() == '\t' )
            {
                int next_stop = col + stop - ( col % stop );
                s.append( next_stop - col, ' ' );
                col = next_stop;

                continue;
            }

            if ( sv.front() == '\n' )
                col = 0;
            else
                ++col;

            s.push_back( sv.front() );
        }

        return s;
    }
};

} // namespace qthu
