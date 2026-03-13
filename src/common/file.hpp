#pragma once

#include <fstream>
#include <sstream>
#include <cstdint>
#include <vector>
#include <string>

inline std::string read_file( std::istream& in )
{
    std::string out;
    int pos = 0;

    while ( in )
    {
        out.resize( pos + 1024 );
        in.read( out.data() + pos, 1024 );
        out.resize( pos += in.gcount() );
    }

    return out;
}

inline std::string read_file( std::string path )
{
    std::ifstream file( path, std::ios::in );
    if ( !file.is_open() )
        throw std::runtime_error( "Couldn't open file at: " + path );

    std::stringstream sstream;
    sstream << file.rdbuf();
    return sstream.str();
}

inline void dump_bytes( std::string path, const std::vector< std::uint8_t >& bytes )
{
    std::ofstream out( path, std::ios::binary );
    if ( !out.is_open() )
        throw std::runtime_error( "Couldn't open output file " + path );

    out.write( reinterpret_cast< const char* >( bytes.data() ), bytes.size() );
}