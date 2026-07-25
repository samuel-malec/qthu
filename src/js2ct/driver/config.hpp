#pragma once

#include <cstring>
#include <iostream>
#include <string>
#include <stdexcept>

namespace qthu::js2ct
{

struct config
{
    std::string in_path;
    std::string out_path;
    bool emit_ast;
    bool emit_hir;
    bool emit_cthu;
    bool emit_lin;
};

inline void help()
{
    std::cout << "Usage:\n"
              << "./js2ct file.js [-o out.ct]\n"
              << "-A (print ast)\n"
              << "-H (print hir)\n"
              << "-L (print linear ir)\n"
              << "-C (print cthulhu)\n"; 
}

inline config parse_config( int argc, char* const* argv )
{
    if ( argc < 1 )
        throw std::runtime_error( "Usage: ./js2ct file.js [-o out.ct]\n" );
    
    if ( strcmp( argv[ 0 ], "-h" ) == 0 )
    {
        help();
        exit( 0 ); 
    }

    std::string file_in = argv[ 0 ];
    std::string file_out = "out.ct";
    bool _emit_ast = false;
    bool _emit_cthu = false;
    bool _emit_hir = false;
    bool _emit_lin = false;
    
    for ( int i = 1; i < argc; ++i )
    {
        if ( strcmp( argv[ i ], "-o" ) == 0 )
        {
            if ( i >= argc - 1 )
                throw std::runtime_error( "missing argument of -o" );

            ++i;
            file_out = argv[ i ];
        }
        else if ( strcmp( argv[ i ], "-A" ) == 0 )
        {
            _emit_ast = true;
            continue;
        }
        else if ( strcmp( argv[ i ], "-H" ) == 0 )
        {
            _emit_hir = true;
            continue;
        }
        else if ( strcmp( argv[ i ], "-L" ) == 0 )
        {
            _emit_lin = true;
            continue;
        }
        else if ( strcmp( argv[ i ], "-C" ) == 0 )
        {
            _emit_cthu = true;
            continue;
        }

        throw std::runtime_error( "invalid flag" );
    }

    return { 
            .in_path = file_in,
            .out_path = file_out,
            .emit_ast = _emit_ast,
            .emit_hir = _emit_hir,
            .emit_cthu = _emit_cthu,
            .emit_lin = _emit_lin
        };
}

}
