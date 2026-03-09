#pragma once

#include <string>

namespace qthu
{

struct Config
{
    std::string in_name;
    std::string out_name;
};

inline Config parse_config( int argc, const char** argv )
{
    if( ( argc != 0 && argc != 3 ) || ( argc == 3 && std::string( argv[ 1 ] ) != "-o" ) )
        throw std::runtime_error( "Usage: qasm in_file.txt -o out_file.qbc\n" );

    std::string name_in = argv[ 0 ];
    if( argc == 1 )
        return Config{ argv[ 1 ], std::string( "out.qbc" ) };

    return Config{ argv[ 0 ], argv[ 2 ] };
}

} // namespace qthu
