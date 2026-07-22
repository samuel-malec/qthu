#include <cstring>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

#include "driver/compiler.hpp"

int main( int argc, char* const* argv )
{
    using namespace jsc;
    --argc;
    ++argv;

    try
    {
        auto conf = parse_config( argc, argv );
        compiler comp{};
        comp.run( conf ); 
    }
    
    catch( const std::exception& e )
    {
        std::cerr << "\033[1;31mexception:\033[m " << e.what() << "\n";
        return -1;
    }
}