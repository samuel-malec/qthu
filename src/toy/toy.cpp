#include <iostream>
#include <stdexcept>
#include <string>
#include <cstring>

#include "codegen.hpp"
#include "../common/file.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "../asm/asmbuilder.hpp"

using config = std::tuple< std::string, std::string >;

config parse_args( int argc, char* const* argv )
{
    std::string out_name = "a.out";

    if ( argc == 0 )
        throw std::runtime_error( "Usage: ./toy in_name [-o out.name]" );

    std::string in_name = argv[ 0 ];
    argc--;
    argv++;
    if ( argc == 2 && strcmp( argv[ 0 ], "-o" ) == 0 )
        return { in_name, argv[ 1 ] };

    if ( argc != 0 )
        throw std::runtime_error( "Usage: ./toy in_name [-o out.name]" );

    return { in_name, out_name };
}

int main( int argc, char* const* argv )
{
    using namespace qthu;
    --argc;
    ++argv;
    try
    {
        const auto& [ in_name, out_name ] = parse_args( argc, argv );
        std::string content = read_file( in_name );

        const auto& tokens = toy::lex( content );
        const auto ast = parse( tokens );
        
        as::asmbuilder builder;
        generate( builder, ast );
        
        std::cout << builder.print_asm() << '\n';
        
        bc::program prog = builder.build();
        prog.write_binary( out_name );
    }
    catch ( const std::exception& e )
    {
        std::cerr << "\033[1;31mexception:\033[m " << e.what() << "\n";
        return 1;
    }

    return 0;
}