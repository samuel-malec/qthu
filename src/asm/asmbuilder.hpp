#pragma once

#include "asm.hpp"
#include "op_builders.hpp"

namespace qthu::as
{

struct asmbuilder
{
    assembly assm;
    function_assembly* current = nullptr;

    asmbuilder& add_function( std::string_view name, uint16_t args, uint16_t locals,
                          uint16_t stack_size )
    {
        current = &assm.add_function();
        current->name = std::string( name );
        current->arg_count = args;
        current->local_count = locals;
        current->stack_size = stack_size;

        current->add_directive( { .mnemonic = "function", .name = std::string( name ) } );
        current->add_directive( { .mnemonic = "args", .value = args } );
        current->add_directive( { .mnemonic = "locals", .value = locals } );
        current->add_directive( { .mnemonic = "stack_size", .value = stack_size } );
        return *this;
    }

    asmbuilder& add_label( std::string_view name )
    {
        ensure_function();
        current->add_label( name );
        return *this;
    }

    asmbuilder& add_instr( std::string_view text )
    {
        ensure_function();
        current->add_instr( instruction::from_string( text ) );
        return *this;
    }

    asmbuilder& add_instr( instruction instr )
    {
        ensure_function();
        current->add_instr( std::move( instr ) );
        return *this;
    }

    std::string print_asm() const
    {
        return assm.print();
    }

    bc::program build()
    {
        return assm.assemble();
    }

    void ensure_function() const
    {
        if ( !current )
            throw std::runtime_error( "call function(...) before adding labels/instructions" );
    }
};

}
