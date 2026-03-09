#pragma once

#include <fstream>
#include <sstream>
#include <cstdint>
#include <vector>
#include <string>

inline std::string load_from_file( std::string path )
{
    std::ifstream file( path, std::ios::in );
    if( !file.is_open() )
        throw std::runtime_error( "Couldn't open file at: " + path );

    std::stringstream sstream;
    sstream << file.rdbuf();
    return sstream.str();
}

inline void dump_bytes( std::string path, const std::vector< std::uint8_t >& bytes )
{
    std::ofstream out( path, std::ios::binary );
    if( !out.is_open() )
        throw std::runtime_error( "Couldn't open output file " + path );

    out.write( reinterpret_cast< const char* >( bytes.data() ), bytes.size() );
}