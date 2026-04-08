#pragma once

#include <cstdint>
#include <iostream>
#include <limits>
#include <map>
#include <set>
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

// todo maybe rename this ? 
struct resolved_insn
{
    enum class kind_t
    {
        builtin,
        fn_ref,
        fn_call,
        fn_opt,
        fn_join,
    };

    kind_t kind = kind_t::builtin;
    atom structure{};
    atom operation{};
    atom target{};
    uint32_t target_fn_id = std::numeric_limits< uint32_t >::max();
    std::vector< atom > in;
    std::vector< atom > out;

    std::vector< uint32_t > slots_in;
    std::vector< uint32_t > slots_out;
};

struct fn_meta
{
    uint32_t id = 0;
    insn_key key{};
    std::vector< atom > in;
    std::vector< atom > out;
    std::vector< resolved_insn > body;
    uint32_t slot_size = 0;
};

struct cthuir
{
    symtab& st;
    std::vector< fn_meta > fns{};
    std::map< insn_key, uint32_t > key_fn{};
    std::map< insn_key, atom > builtins{};

    // initial pass to resolve all function names etc.
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
                    func.in,
                    func.out,
                };

                key_fn[ metadata.key ] = metadata.id;
                fns.push_back( std::move( metadata ) );
            }
        }
    }

    void load_builtins()
    {
        for ( const auto& [ satom, structure ] : st.structures )
            for ( const auto& [ op, builtin_name ] : structure.builtin_ops )
                builtins[{ satom, op }] = builtin_name;
    }

    void build_slots()
    {
        for ( auto& meta : fns )
        {
            auto& structure = st.structures.at( meta.key.stru );
            auto& function = structure.functions.at( meta.key.op );

            uint32_t max_slots = 0;
            std::map< atom, std::vector< uint32_t > > stack_slots;
            
            // todo this is kinda iffy
            uint32_t max_slot_num = 512;
            std::vector< uint32_t > free_slots{};
            for ( int i = 511; i >= 0; -- i )
                free_slots.push_back( i );
            
            auto aloc_slot = [ &max_slots, &max_slot_num, &free_slots, &stack_slots ] 
                             ( std::vector< uint32_t>& slot_sink , atom stck_name )
            {
                uint32_t slot = free_slots.empty() ? max_slot_num++ : free_slots.back();
                slot_sink.push_back( slot );
                if ( free_slots.empty() )
                    free_slots.pop_back();
                stack_slots[ stck_name ].push_back( slot ); 
                max_slots = std::max( max_slots, slot );
            };

            auto free_slot = [ &free_slots, &stack_slots ] 
                            ( std::vector< uint32_t >& slot_sink, atom stck_name )
            {
                uint32_t slot = stack_slots[ stck_name ].back();
                slot_sink.push_back( slot );
                free_slots.push_back( slot );
                stack_slots[ stck_name ].pop_back();
            };

            std::vector< uint32_t > dev_null{};
            for ( auto& in_atom : meta.in )
                aloc_slot( dev_null, in_atom );

            for ( auto& insn : meta.body )
            {
                for ( auto& iatom : insn.in )
                    free_slot( insn.slots_in, iatom);

                for ( auto& oatom : insn.out )
                    aloc_slot( insn.slots_out, oatom );
            }
        }
    }

    resolved_insn build_resolved( const insn_t& insn, resolved_insn::kind_t kind );

    resolved_insn classify( const insn_t& insn ) const
    {
        auto op_name = st.name_of( insn.operation );
        if ( op_name == "call" )
            return resolved_insn{
                .kind = resolved_insn::kind_t::fn_call,
                .structure = insn.structure,
                .operation = insn.operation,
                .target = insn.operation,
                .in = insn.in,
                .out = insn.out,
            };

        if ( op_name == "opt" )
            return resolved_insn{
                .kind = resolved_insn::kind_t::fn_opt,
                .structure = insn.structure,
                .operation = insn.operation,
                .target = insn.operation,
                .in = insn.in,
                .out = insn.out,
            };

        if ( op_name == "join" && st.name_of( insn.structure ).starts_with( "f" ) )
            return resolved_insn{
                .kind = resolved_insn::kind_t::fn_join,
                .structure = insn.structure,
                .operation = insn.operation,
                .target = insn.operation,
                .in = insn.in,
                .out = insn.out,
            };

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
                resolved_insn::kind_t::fn_ref,
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

            meta.body.clear();
            meta.body.reserve( function.body.size() );

            for ( const auto& insn : function.body )
                meta.body.push_back( classify( insn ) );
        }
    }

    void print() const
    {
        std::cout << "\nFunction Registry (" << fns.size() << " functions):\n";
        for ( const auto& metadata : fns )
        {
            std::cout << "  [" << metadata.id << "] "
                      << st.name_of( metadata.key.stru ) << "::"
                      << st.name_of( metadata.key.op ) << '\n';

            for ( const auto& insn : metadata.body )
            {
                std::cout << "    - ";
                switch ( insn.kind )
                {
                    case resolved_insn::kind_t::builtin:
                        std::cout << "builtin ";
                        break;
                    case resolved_insn::kind_t::fn_ref:
                        std::cout << "fn_ref ";
                        break;
                    case resolved_insn::kind_t::fn_call:
                        std::cout << "fn_call ";
                        break;
                    case resolved_insn::kind_t::fn_opt:
                        std::cout << "fn_opt ";
                        break;
                    case resolved_insn::kind_t::fn_join:
                        std::cout << "fn_join ";
                        break;
                }

                std::cout << st.name_of( insn.structure ) << "::"
                          << st.name_of( insn.operation ) << " -> ";

                if ( insn.kind == resolved_insn::kind_t::builtin )
                    std::cout << st.name_of( insn.target );
                else if ( insn.kind == resolved_insn::kind_t::fn_ref )
                    std::cout << "fn#" << insn.target_fn_id << " (" << st.name_of( insn.target ) << ")";
                else if ( insn.kind == resolved_insn::kind_t::fn_call )
                    std::cout << "call";
                else if ( insn.kind == resolved_insn::kind_t::fn_opt )
                    std::cout << "opt";
                else
                    std::cout << "join";

                std::cout << "  slots_in: [ ";
                for ( int s : insn.slots_in )
                    std::cout<< s << ' ';
                std::cout << "] slots_out: [ ";
                for ( int s : insn.slots_out )
                    std::cout << s << " ";
                std::cout << "]\n";
            }
        }
    }

    void lower()
    {
        build_fns();
        load_builtins();
        build_fn_bodies();
        build_slots();
        print();
    }
};

}
