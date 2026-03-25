#include <iostream>
#include <stdexcept>
#include <string>
#include <cstring>

#include "../common/file.hpp"
#include "lexer.hpp"

using config = std::tuple< std::string, std::string >;

config parse_args( int argc, char* const* argv )
{
    std::string out_name = "a.out";

    if ( argc == 0 )
        throw std::runtime_error( "Usage: ./cthucc in_name.ct [-o out.name]" );

    std::string in_name = argv[ 0 ];
    argc--;
    argv++;
    if ( argc == 2 && strcmp( argv[ 0 ], "-o" ) == 0 )
        return { in_name, argv[ 1 ] };

    if ( argc != 0 )
        throw std::runtime_error( "Usage: ./cthucc in_name.ct [-o out.name]" );

    return { in_name, out_name };
}


int main( int argc, char* const* argv )
{
    using namespace cthu;
    --argc;
    ++argv;

    try
    {
        // TODO: parse prelude
        const auto& [ in_name, out_name ] = parse_args( argc, argv );
        std::string content = read_file( in_name );
        source_ptr ptr = std::make_shared< source_file >( in_name, content );
        const auto& toks = lex( ptr );
        for ( const auto& t : toks )
            std::cout << t << '\n'; 

    }
    catch( const std::exception& e )
    {
        std::cerr << "\033[1;31mexception:\033[m " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}