#pragma once

#include <cstdint>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <vector>

#include "symtab.hpp"
#include "types.hpp"

namespace cthu
{

struct insn_key
{
    atom stru;
    atom op;

    bool operator<( const insn_key& rhs ) const
    {
        if ( stru.index != rhs.stru.index )
            return stru.index < rhs.stru.index;
        return op.index < rhs.op.index;
    }
};

struct resolved_insn
{
    enum class kind_t
    {
        builtin,
        fn_call,
    };

    kind_t kind = kind_t::builtin;
    atom structure{};
    atom operation{};
    atom target{};
    uint32_t target_fn_id = std::numeric_limits< uint32_t >::max();
    std::vector< atom > in;
    std::vector< atom > out;
};

struct fn_meta
{
    uint32_t id = 0;
    insn_key key{};
    uint32_t in_arity = 0;
    uint32_t out_arity = 0;
    std::vector< resolved_insn > rinsn;
};

struct cthuir
{
    symtab& st;
    std::vector< fn_meta > fns{};
    std::map< insn_key, uint32_t > key_fn{};
    std::map< insn_key, atom > builtins{};

    void build_fns()
    {
        fns.clear();
        key_fn.clear();

        for ( const auto& [ struct_atom, structure ] : st.structures )
        {
            for ( const auto& [ func_atom, func ] : structure.functions )
            {
                auto metadata = fn_meta{
                    static_cast< uint32_t >( fns.size() ),
                    { struct_atom, func_atom },
                    static_cast< uint32_t >( func.in.size() ),
                    static_cast< uint32_t >( func.out.size() ),
                };

                key_fn[ metadata.key ] = metadata.id;
                fns.push_back( std::move( metadata ) );
            }
        }
    }

    void build_builtins()
    {
        builtins.clear();

        for ( const auto& [ satom, structure ] : st.structures )
            for ( const auto& [ op, builtin_name ] : structure.builtin_ops )
                builtins[{ satom, op }] = builtin_name;
    }

    resolved_insn classify( const insn_t& insn ) const
    {
        insn_key key{ insn.structure, insn.operation };

        if ( auto it = builtins.find( key ); it != builtins.end() )
        {
            return resolved_insn{
                resolved_insn::kind_t::builtin,
                insn.structure,
                insn.operation,
                it->second,
                std::numeric_limits< uint32_t >::max(),
                insn.in,
                insn.out,
            };
        }

        if ( auto it = key_fn.find( key ); it != key_fn.end() )
        {
            return resolved_insn{
                resolved_insn::kind_t::fn_call,
                insn.structure,
                insn.operation,
                insn.operation,
                it->second,
                insn.in,
                insn.out,
            };
        }

        throw std::runtime_error(
            std::string( "unresolved instruction: " ) +
            std::string( st.name_of( insn.structure ) ) + "::" +
            std::string( st.name_of( insn.operation ) )
        );
    }

    void build_fn_bodies()
    {
        for ( auto& meta : fns )
        {
            auto& structure = st.structures.at( meta.key.stru );
            auto& function = structure.functions.at( meta.key.op );

            meta.rinsn.clear();
            meta.rinsn.reserve( function.body.size() );

            for ( const auto& insn : function.body )
                meta.rinsn.push_back( classify( insn ) );
        }
    }

    void print() const
    {
        std::cout << "\nFunction Registry (" << fns.size() << " functions):\n";
        for ( const auto& metadata : fns )
        {
            std::cout << "  [" << metadata.id << "] "
                      << st.name_of( metadata.key.stru ) << "::"
                      << st.name_of( metadata.key.op )
                      << " (" << metadata.in_arity << " in, "
                      << metadata.out_arity << " out)\n";

            for ( const auto& insn : metadata.rinsn )
            {
                std::cout << "    - ";
                if ( insn.kind == resolved_insn::kind_t::builtin )
                    std::cout << "builtin ";
                else
                    std::cout << "fn-call #" << insn.target_fn_id << " ";

                std::cout << st.name_of( insn.structure ) << "::"
                          << st.name_of( insn.operation ) << " -> "
                          << st.name_of( insn.target ) << '\n';
            }
        }

        std::cout << "\nBuiltins:\n";
        for ( const auto& [ key, builtin_atom ] : builtins )
            std::cout << "  " << st.name_of( key.stru ) << "::" << st.name_of( key.op )
                      << " = " << st.name_of( builtin_atom ) << '\n';
    }

    void lower()
    {
        build_fns();
        build_builtins();
        build_fn_bodies();
        print();
    }
};

}
