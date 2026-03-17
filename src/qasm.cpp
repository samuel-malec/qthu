#include <iostream>
#include <fstream>
#include <string>
#include <cstring>

#include "common/file.hpp"
#include "common/document_store.hpp"
#include "asm/qassembly.hpp"
#include "bytecode/program.hpp"

int main( int argc, const char** argv )
{
    --argc, ++argv;

    using namespace qthu;
    document_store docstore;
    assembly assm;

    const document* current_doc = nullptr;
    const char* out_name = "qthu.qbc";

    try
    {
        if ( argc == 0 )
            current_doc = &docstore.load_from_stdin();

        while ( current_doc || argc > 0 )
        {
            if ( argc > 1 && strcmp( *argv, "-o" ) == 0 )
            {
                out_name = argv[ 1 ];
                argc -= 2;
                argv += 2;
                continue;
            }

            if ( !current_doc )
                current_doc = &docstore.load_from_file( *argv++ ), --argc;

            assm.add_document( *current_doc );
            current_doc = nullptr;
        }

        std::cout << "=== Assembly Items ===\n";
        std::cout << assm.print() << "\n";

        auto prog = assm.assemble();

        std::cout << "\n=== Generated Program ===\n";
        std::cout << prog.print() << "\n";

        std::cout << "\n=== Bytecode Hex Dump ===\n";
        std::cout << prog.hex_dump() << "\n";

        prog.write_binary( out_name );
        std::cout << "Written to: " << out_name << "\n";
    }
    catch ( const std::exception& e )
    {
        std::cout << "Error: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
