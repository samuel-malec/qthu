#include <cstring>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

#include "frontend/token.hpp"
#include "../common/file.hpp"

using config = std::pair< std::string, std::string >;

config parse_config( int argc, char* const* argv )
{
    std::string out = "a.ct";
    if ( argc == 0 )
        throw std::runtime_error( "usage: ./jscc in.js [-o out.js]" );
    
    std::string in = argv[ 0 ];
    argc--;
    argv++;
    if ( argc == 2 && strcmp( argv[ 0 ], "-o" ) == 0 )
        return { in, argv[ 1 ] };
    
    if ( argc != 0 )
        throw std::runtime_error( "usage: ./jscc in.js [-o out.js]" );

    return { in, out };
}

int main( int argc, char* const* argv )
{
    --argc;
    ++argv;

    try
    {
        const auto& [ in_name, out_name ] = parse_config( argc, argv );
        jscc::source_ptr doc = std::make_shared< jscc::source_file >( in_name, read_file( in_name ) ); 
    }
    
    catch( const std::exception& e )
    {
        std::cerr << "\033[1;31mexception:\033[m " << e.what() << "\n";
        return -1;
    }
}