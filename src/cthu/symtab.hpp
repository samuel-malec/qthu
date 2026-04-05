#pragma once

#include <iostream>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "types.hpp"

namespace cthu
{

struct symtab
{
    std::map< std::string, uint32_t, std::less<> > map;
    std::vector< std::string > reverse;

    std::map< atom, type_t > types;
    std::map< atom, signature_t > signatures;
    std::map< atom, structure_t > structures;

    type_ptr get_type( atom a ) { return &types[ a ]; }
    signature_ptr get_signature( atom a ) { return &signatures[ a ]; }
    structure_ptr get_structure( atom a ) { return &structures[ a ]; }

    atom get( std::string_view name )
    {
        atom rv;

        if ( auto it = map.find( name ); it != map.end() )
            rv.index = it->second;
        else
        {
            rv.index = map.size();
            std::string name_str{ name };
            map.emplace( name_str, rv.index );
            reverse.push_back( name_str );
        }
        return rv;
    }

    std::string_view name_of( atom a ) const
    {
        if ( a.index >= reverse.size() )
            return "unknown";
        return reverse[ a.index ];
    }

    std::string_view name_of( uint32_t idx ) const
    {
        if ( idx >= reverse.size() )
            return "unknown";
        return reverse[ idx ];
    }

    bool has_type( std::string_view name )
    {
        return types.contains( get( name ) );
    }

    bool has_structure( std::string_view name )
    {
        return structures.contains( get( name ) );
    }

    bool has_signature( std::string_view name )
    {
        return signatures.contains( get( name ) );
    }

    type_ptr get_type( std::string_view name )
    {
        return get_type( get( name ) );
    }

    signature_ptr get_signature( std::string_view name )
    {
        return get_signature( get( name ) );
    }

    structure_ptr get_structure( std::string_view name )
    {
        return get_structure( get( name ) );
    }
};

inline void print_parsed_program( cthu::symtab &st )
{
    std::cout << "Signatures:\n";
    for ( auto& [ atom, sig ] : st.signatures )
    {
        std::cout << "  " << st.name_of( atom ) << "[";
        for ( size_t i = 0; i < sig.args.size(); ++i )
        {
            if ( i > 0 ) std::cout << ", ";
            std::cout << st.name_of( sig.args[i] );
        }
        std::cout << "]\n";
        
        std::cout << "    Operations:\n";
        for ( auto& [ op_atom, op_def ] : sig.defs )
        {
            std::cout << "      " << st.name_of( op_atom ) << " ∷ ";
            for ( size_t i = 0; i < op_def.in.size(); ++i )
            {
                if ( i > 0 ) std::cout << " × ";
                std::cout << st.name_of( op_def.in[i] );
            }
            std::cout << " → ";
            for ( size_t i = 0; i < op_def.out.size(); ++i )
            {
                if ( i > 0 ) std::cout << " × ";
                std::cout << st.name_of( op_def.out[i] );
            }
            std::cout << "\n";
        }
    }

    std::cout << "\nStructures:\n";
    for ( auto& [ atom, structure ] : st.structures )
    {
        std::cout << "  structure " << st.name_of( atom ) << "\n";
        
        std::cout << "    Signatures: ";
        for ( size_t i = 0; i < structure.signatures.size(); ++i )
        {
            if ( i > 0 ) std::cout << ", ";
            std::cout << st.name_of( structure.signatures[i].signature );
        }
        std::cout << "\n";
        
        std::cout << "    Functions:\n";
        for ( auto& [ op_id, fn ] : structure.functions )
        {
            std::cout << "    λ" << st.name_of( op_id ) << '\n';
            for ( auto& insn : fn.body )
            {
                auto& structure_atom = insn.structure;
                auto& op_atom = insn.operation;
                cthu::structure_t& struc = st.structures[ structure_atom ];
                
                if ( struc.builtin_ops.contains( op_atom ) )
                    std::cout << "        " << st.name_of( struc.builtin_ops[ op_atom ] ) << " ";
                else 
                    std::cout << "        " << st.name_of( insn.structure ) << " " << st.name_of( insn.operation );
                
                for ( auto& arg : insn.in )
                    std::cout << " " << st.name_of( arg );
                
                if ( insn.out.empty() )
                {
                    std::cout << "\n";
                    continue;
                }
             
                std::cout << " -> ";
             
                for ( auto& arg : insn.out )
                    std::cout << " " << st.name_of( arg );
                std::cout << "\n";
            }
        }
    }
}



}
