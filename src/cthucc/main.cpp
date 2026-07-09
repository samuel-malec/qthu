#include <iostream>
#include <stdexcept>
#include <string>
#include <cstring>
#include <filesystem>

#include "../asm/asmbuilder.hpp"
#include "../common/file.hpp"
#include "codegen.hpp"
#include "reader.hpp"
#include "ir.hpp"

using config = std::pair< std::string, std::string >;
namespace fs = std::filesystem;

using namespace qthu;

config parse_args( int argc, char* const* argv )
{
    std::string out_name = "a.qbc";

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

cthucc::diag parse_source( const fs::path &path, cthucc::symtab &st )
{
    std::string content = read_file( path.string() );
    auto ptr = std::make_shared< cthucc::source_file >( path.string(), std::move( content ) );
    cthucc::reader r{ ptr, st };
    return r.parse();
}

std::pair< fs::path, fs::path > support_files_for( const fs::path &input )
{
    auto local_prelude = input.parent_path() / "prelude.ct";
    auto local_builtins = input.parent_path() / "builtins.ct";
    if ( fs::exists( local_prelude ) && fs::exists( local_builtins ) )
        return { local_prelude, local_builtins };

    return { "src/cthu/prelude.ct", "src/cthu/builtins.ct" };
}

void throw_on_diag( cthucc::diag err )
{
    if ( !err )
        return;

    brq::string_builder b;
    err->print( b );
    throw std::runtime_error( std::string( b.data() ) );
}

int main( int argc, char* const* argv )
{
    using namespace qthu;
    --argc;
    ++argv;
    try
    {
        const auto& [ in_name, out_name ] = parse_args( argc, argv );

        cthucc::symtab st;
        auto input = fs::path( in_name );
        auto [ prelude, builtins ] = support_files_for( input );

        throw_on_diag( parse_source( prelude, st ) );
        throw_on_diag( parse_source( builtins, st ) );
        throw_on_diag( parse_source( input, st ) );

        cthucc::program prog{ st };
        prog.lower_to_ir();
        as::asmbuilder builder{};
        cthucc::codegen cg{ prog, builder };
        bc::program bc_prog = cg.lower_to_bc();
        bc_prog.write_binary( out_name );
        std::cout << "qjs bytecode written to: " << out_name << "\n";
    }

    catch( const std::exception& e )
    {
        std::cerr << "\033[1;31mexception:\033[m " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
