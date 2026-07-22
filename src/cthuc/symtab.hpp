#pragma once

#include <iostream>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "../cthu_core/types.hpp"

namespace qthu::cthuc
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

}
