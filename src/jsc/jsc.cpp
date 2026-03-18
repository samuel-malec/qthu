#include <iostream>
#include <stdexcept>
#include <string>
#include <cstring>

#include "../common/file.hpp"
#include "lexer.hpp"
#include "parser.hpp"

using config = std::tuple< std::string, std::string >;

config parse_args( int argc, const char** argv )
{
    std::string out_name = "a.out";

    if ( argc == 0 )
        throw std::runtime_error( "Usage: ./jsc in_name [-o out.name]" );

    std::string in_name = argv[ 0 ];
    argc--;
    argv++;
    if ( argc == 2 && strcmp( argv[ 0 ], "-o" ) == 0 )
        return { in_name, argv[ 1 ] };

    if ( argc != 0 )
        throw std::runtime_error( "Usage: ./jsc in_name [-o out.name]" );

    return { in_name, out_name };
}

int main( int argc, const char** argv )
{
    --argc;
    ++argv;
    try
    {
        const auto& [ in_name, out_name ] = parse_args( argc, argv );
        std::string content = read_file( in_name );
        const auto& tokens = jsc::lex( content );
        const auto ast = jsc::parse( tokens );
        std::cout << jsc::print( ast );
    }
    catch ( const std::exception& e )
    {
        std::cout << e.what() << '\n';
        return 1;
    }

    return 0;
}