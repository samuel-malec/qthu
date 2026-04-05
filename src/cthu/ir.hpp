#pragma once

#include <cstdint>
#include <vector>
#include <map>
#include "types.hpp"
#include "symtab.hpp"

namespace cthu
{

struct structured_op
{
    atom structure;
    atom op;

    bool operator<( const structured_op& rhs ) const
    {
        if ( structure.index == rhs.structure.index )
            return op.index < rhs.op.index;
        return structure.index < rhs.structure.index;
    }
};
    
struct fn_meta
{
    uint32_t id;
    structured_op sop;
    uint32_t in_arity;
    uint32_t out_arity;
    uint32_t bc_offset;
};

struct fn_registry
{
    std::vector< fn_meta > functions;
    std::map< structured_op, uint32_t > id_map;

    uint32_t size() const { return functions.size(); }
};


inline fn_registry build_from_symtab( const symtab& st )
{
    fn_registry res{};
    for ( const auto& [ struct_atom, structure ] : st.structures )
        for ( const auto& [ func_atom, func ] : structure.functions )
        {
            auto metadata = fn_meta{
                res.size(),
                struct_atom,
                func_atom,
                static_cast< uint32_t >( func.in.size() ),
                static_cast< uint32_t >( func.out.size() ),
                0
            };
            
            res.functions.push_back( metadata );
            structured_op key{ struct_atom, func_atom };
            res.id_map[key] = metadata.id;
        }

    return res;
}

inline void print_function_registry( const cthu::fn_registry& fr, const cthu::symtab& st )
{
    std::cout << "\nFunction Registry (" << fr.size() << " functions):\n";
    for ( const auto& metadata : fr.functions )
    {
        std::cout << "  [" << metadata.id<< "] "
                  << st.name_of( metadata.sop.structure ) << "::" 
                  << st.name_of( metadata.sop.op )
                  << " (" << metadata.in_arity << " in, "
                  << metadata.out_arity << " out)\n";
    }
}

}
