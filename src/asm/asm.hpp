#pragma once

#include <string>
#include <sstream>
#include <vector>
#include <map>

#include "../bytecode/program.hpp"
#include "../common/document.hpp"
#include "directive.hpp"
#include "instruction.hpp"

namespace qthu::as
{

using symtab_t = std::map< std::string, uint32_t >;

struct function_assembly
{
    struct item
    {
        enum kind_t
        {
            instruction,
            label,
            directive,
        } kind;

        as::instruction instr;
        as::directive dir;
        std::string name;

        document::span lit;
        uint16_t size = 0;

        uint16_t length() const
        {
            if ( kind == instruction )
                return size;
            return 0;
        }
    };

    std::string name;
    uint16_t arg_count = 0;
    uint16_t local_count = 0;
    uint16_t stack_size = 0;

    std::vector< item > items;

    auto& add( item&& i )
    {
        items.push_back( std::move( i ) );
        return items.back();
    }

    auto& add_instr( instruction i, document::span lit = {} )
    {
        return add( { .kind = item::instruction, .instr = std::move( i ), .lit = lit, .size = i.size } );
    }

    auto& add_label( std::string_view label_name, document::span lit = {} )
    {
        return add( { .kind = item::label, .name = std::string{ label_name }, .lit = lit } );
    }

    auto& add_directive( directive dir, document::span lit = {} )
    {
        return add( { .kind = item::directive, .dir = dir, .name = dir.mnemonic, .lit = lit } );
    }

    symtab_t collect_labels() const;
    bc::function_bytecode assemble() const;
    std::string print() const;
};

struct assembly
{
    std::vector< function_assembly > functions;

    function_assembly& add_function()
    {
        functions.emplace_back();
        return functions.back();
    }

    function_assembly& current_function()
    {
        if ( functions.empty() )
            throw std::runtime_error( "no current function" );
        return functions.back();
    }

    function_assembly::item* add_line( document::span );
    void add_document( const document& );
    bc::program assemble();

    std::string print() const
    {
        std::ostringstream out;
        for ( const auto& func : functions )
            out << func.print() + "\n";
        return out.str();
    }
};

}
