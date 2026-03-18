#include <iostream>

#include "asm/asmbuilder.hpp"

int main()
{
    qthu::asmbuilder builder;
    builder.function( "main", 0, 0, 5 );
    builder.instr( "push_i32 6" );
    // builder.instr( "goto sike" );
    builder.instr( "push_i32 2" );
    builder.instr( "add" );
    // builder.label( "sike" );
    builder.instr( "return" );
    std::cout << builder.print_asm();
    qthu::program prog = builder.build();
    prog.write_binary( "demo.qbc" );
}
