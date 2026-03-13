#include <algorithm>
#include <cctype>
#include <iostream>
#include <stdexcept>
#include <format>

#include "opcodes.hpp"
#include "qassembly.hpp"
#include "../bytecode/program.hpp"
#include "../common/parser.hpp"

namespace qthu
{

void assembly::add_document( const document& doc )
{
    for ( int i = 1; i <= doc.line_count(); ++i )
        add_line( doc.ref( doc.line( i ) ) );
}

function_assembly::item* assembly::add_line( document::span span )
{
    auto line = span.sv;
    line_parser p{ line };
    p.drop_blanks();
    p.trim_blanks();
    span.sv = p.sv;

    if ( p.sv.empty() )
        return nullptr;

    // directive
    if ( p.try_drop( "." ) )
    {
        try
        {
            auto dir = directive::from_string( p.sv );

            if ( dir.mnemonic == "function" )
            {
                auto& func = add_function();
                func.name = dir.name;
                return &func.add_directive( dir, span );
            }

            auto& func = current_function();

            if ( dir.mnemonic == "args" )
                func.arg_count = dir.value;
            else if ( dir.mnemonic == "locals" )
                func.local_count = dir.value;
            else if ( dir.mnemonic == "stack_size" )
                func.stack_size = dir.value;

            return &func.add_directive( dir, span );
        }
        catch ( std::exception& e )
        {
            std::cout << e.what() << '\n';
            throw;
        }
    }

    // label
    if ( p.try_trim( ":" ) )
    {
        p.trim_blanks();
        auto label = p.shift_label();

        if ( !p.sv.empty() )
            throw std::runtime_error( std::format( "invalid label: '{}', remaining: '{}'",
                                                   std::string{ label }, std::string{ p.sv } ) );

        span.sv = label;
        return &current_function().add_label( label, span );
    }

    // instruction
    try
    {
        return &current_function().add_instr( instruction::from_string( p.sv ), span );
    }
    catch ( std::exception& e )
    {
        std::cout << e.what() << '\n';
        std::cout << " in file: " << span.doc().filename << '\n';
        throw;
    }
}

symtab_t function_assembly::collect_labels() const
{
    symtab_t labels;
    uint32_t pc = 0;

    for ( const auto& item : items )
    {
        if ( item.kind == item::label )
        {
            if ( labels.count( item.name ) )
                throw std::runtime_error(
                    std::format( "duplicate label '{}' in function '{}'", item.name, name ) );
            labels[ item.name ] = pc;
        }
        pc += item.length();
    }

    return labels;
}

function_bytecode function_assembly::assemble() const
{
    function_bytecode func;
    func.name = name;
    func.arg_count = arg_count;
    func.local_count = local_count;
    func.stack_size = stack_size;
    func.local_labels = collect_labels();

    uint32_t pc = 0;
    for ( const auto& item : items )
    {
        if ( item.kind != item::instruction )
            continue;

        instruction instr = item.instr;
        if ( !instr.resolve( func.local_labels, pc ) )
        {
            throw std::runtime_error( std::format(
                "unresolved label in instruction '{}' in function '{}'", instr.mnemonic, name ) );
        }

        auto bytes = instr.to_bytes();
        func.bytecode.insert( func.bytecode.end(), bytes.begin(), bytes.end() );
        pc += item.size;
    }

    return func;
}

program assembly::assemble()
{
    program prog;
    for ( const auto& func_asm : functions )
    {
        auto func_bc = func_asm.assemble();
        prog.functions.push_back( std::move( func_bc ) );
    }

    return prog;
}

std::string function_assembly::print() const
{
    std::string result;
    result += std::format( "Function: {} (args: {}, locals: {}, stack: {})\n", name, arg_count,
                           local_count, stack_size );
    for ( const auto& item : items )
    {
        switch ( item.kind )
        {
        case item::instruction:
            result +=
                std::format( "    [instruction] {} (size: {})\n", item.instr.mnemonic, item.size );
            break;
        case item::label:
            result += std::format( "  [label] {}:\n", item.name );
            break;
        }
    }
    return result;
}


} // namespace qthu
