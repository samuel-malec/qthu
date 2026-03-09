#include <iostream>
#include <fstream>
#include <string>

#include "config.hpp"
#include "common/file_store.hpp"
#include "qasm.hpp"

int main( int argc, const char** argv )
{
    --argc, ++argv;
    try
    {
        qthu::Config config = qthu::parse_config( argc, argv );
        std::string content = load_from_file( config.in_name );
        const auto& bytecode = qthu::assemble_file( content );
        dump_bytes( config.out_name, bytecode );
    }
    catch( const std::exception& e )
    {
        std::cout << "Error: " << e.what() << '\n';
        return 1;
    }
    return 0;
}