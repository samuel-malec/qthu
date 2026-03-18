#include <iostream>

#include "asm/asmbuilder.hpp"

int main()
{
    qthu::asmbuilder builder;
    builder.add_function( "main", 0, 0, 5 );
    builder.add_instr( "push_i32 6" );
    // builder.instr( "goto sike" );
    builder.add_instr( "push_i32 2" );
    builder.add_instr( "add" );
    // builder.add_label( "sike" );
    builder.add_instr( "return" );
    std::cout << builder.print_asm();
    qthu::program prog = builder.build();
    prog.write_binary( "demo.qbc" );
}
