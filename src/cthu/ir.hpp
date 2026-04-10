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

namespace qthu::cthu
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
};

struct lowered_insn
{
    resolved_insn resolved;
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
    std::vector< lowered_insn > lowered;
    
    std::vector< uint32_t > sig_in_slots;
    std::vector< uint32_t > sig_out_slots;
    uint32_t slot_size = 0;
};

struct program
{
    symtab& st;
    std::vector< fn_meta > fns{}; 
    std::map< insn_key, uint32_t > key_fn{};
    std::map< insn_key, atom > builtins{};

    void collect_fns()
    {
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

    void collect_builtins()
    {
        for ( const auto& [ satom, structure ] : st.structures )
            for ( const auto& [ op, builtin_name ] : structure.builtin_ops )
                builtins[{ satom, op }] = builtin_name;
    }

    void aloc_slots()
    {
        for ( auto& meta : fns )
        {
            std::map< atom, std::vector< uint32_t > > versions;
            uint32_t next_slot = 0;
            std::vector< uint32_t > free_slots;

            auto alloc_slot = [&]() -> uint32_t
            {
                if ( !free_slots.empty() )
                {
                    const uint32_t s = free_slots.back();
                    free_slots.pop_back();
                    return s;
                }
                return next_slot++;
            };

            auto pop_version = [&]( atom name ) -> uint32_t
            {
                auto it = versions.find( name );
                if ( it == versions.end() || it->second.empty() )
                        throw std::runtime_error(
                        std::string( "slot alloc underflow for name: " ) +
                        std::string( st.name_of( name ) ) );
                
                const uint32_t s = it->second.back();
                it->second.pop_back();
                return s;
            };

            auto push_version = [&]( atom name, uint32_t slot )
            {
                versions[name].push_back( slot );
            };

            for ( const auto& p : meta.in )
            {
                const uint32_t s = alloc_slot();
                push_version( p, s );
                meta.sig_in_slots.push_back( s );
            }

            for ( const auto& insn : meta.body )
            {
                lowered_insn low;
                low.resolved = insn;

                std::vector< uint32_t > to_free;
                to_free.reserve( insn.in.size() );

                for ( const auto& iatom : insn.in )
                {
                    const uint32_t s = pop_version( iatom );
                    low.slots_in.push_back( s );
                    to_free.push_back( s );
                }

                for ( const uint32_t s : to_free )
                    free_slots.push_back( s );

                for ( const auto& oatom : insn.out )
                {
                    const uint32_t s = alloc_slot();
                    push_version( oatom, s );
                    low.slots_out.push_back( s );
                }

                meta.lowered.push_back( std::move( low ) );
            }

            for ( const auto& o : meta.out )
            {
                auto it = versions.find( o );
                if ( it == versions.end() || it->second.empty() )
                {
                    throw std::runtime_error(
                        std::string( "function output name not produced: " ) +
                        std::string( st.name_of( meta.key.stru ) ) + "::" +
                        std::string( st.name_of( meta.key.op ) ) + " -> " +
                        std::string( st.name_of( o ) ) );
                }
                meta.sig_out_slots.push_back( it->second.back() );
            }

            meta.slot_size = next_slot;
        }
    }

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

    void resolve_instructions()
    {
        for ( auto& meta : fns )
        {
            auto& structure = st.structures.at( meta.key.stru );
            auto& function = structure.functions.at( meta.key.op );

            for ( const auto& insn : function.body )
                meta.body.push_back( classify( insn ) );
        }
    }

    void lower_to_ir()
    {
        collect_fns();
        collect_builtins();
        resolve_instructions();
        aloc_slots();
        print();
    }

    void print() const
    {
        for ( auto& fn : fns )
        {
            std::cout << "fn " << st.name_of( fn.key.stru ) << "::" << st.name_of( fn.key.op ) << '\n';
            for ( auto& l : fn.lowered )
            {
                std::cout << "      ";
                switch (l.resolved.kind)
                {
                    case cthu::resolved_insn::kind_t::builtin:
                        std::cout << "(builtin)";
                        break;
                    case cthu::resolved_insn::kind_t::fn_call:
                        std::cout << "(call)";
                        break;
                    case cthu::resolved_insn::kind_t::fn_join:
                        std::cout << "(join)";
                        break;
                    case cthu::resolved_insn::kind_t::fn_opt:
                        std::cout << "(opt)";
                        break;
                    case cthu::resolved_insn::kind_t::fn_ref:
                        std::cout << "(fn_ref)";
                        break;
                    default:
                        std::cout << "(unknown)";
                        break;
                }

                std::cout << " " << st.name_of( l.resolved.target ) << " in: [ ";
                for ( auto& in : l.slots_in )
                    std::cout << in << " ";
                std::cout << "] out: [ ";
                for ( auto& out : l.slots_out )
                    std::cout << out << " ";
                std::cout << "]\n";
            }
        }
    }
};

}
