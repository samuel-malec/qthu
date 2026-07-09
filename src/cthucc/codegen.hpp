#pragma once

#include "../asm/asmbuilder.hpp"
#include "../bytecode/program.hpp"
#include "ir.hpp"

#include <cstdint>
#include <format>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace qthu::cthucc
{

struct fn_patch
{
    std::vector< uint32_t > cpool_funcs;
    std::vector< bc::closure_var > closure_vars;
    bool capture_all = false;
};

struct codegen
{
    cthucc::program& ir;
    as::asmbuilder& builder;
    std::vector< fn_patch > patches;
    std::vector< std::unordered_map< uint32_t, uint16_t > > fn_capture_idx;
    std::size_t label_counter = 0;

    std::string make_label() { return std::format("lab{}", label_counter++ ); }
    
    uint32_t find_main_id() const
    {
        for ( const auto& fn : ir.fns )
            if ( ir.st.name_of( fn.key.stru ) == "main" && ir.st.name_of( fn.key.op ) == "run" )
                return fn.id;
        throw std::runtime_error( "missing main::run function" );
    }

    void ensure_patch( uint32_t idx )
    {
        if ( patches.size() <= idx )
            patches.resize( idx + 1 );
    }

    void emit_builtin( const lowered_insn& insn, uint32_t uid );
    void emit_fn_opt( const lowered_insn& insn );
    void emit_fn_join( const lowered_insn& insn );
    
    void gen_root()
    {
        using namespace qthu::as;
        builder.add_function( "__toplevel__", 0, static_cast< uint16_t >( ir.fns.size() ), 256 );
        ensure_patch( 0 );
        patches[ 0 ].capture_all = true;
        patches[ 0 ].cpool_funcs.reserve( ir.fns.size() );

        for ( uint32_t id = 0; id < ir.fns.size(); ++id )
        {
            patches[ 0 ].cpool_funcs.push_back( 1 + id );
            builder.add_instr( fclosure_( id ) );
            builder.add_instr( put_loc_( id ) );
        }

        const uint32_t main_run_id = find_main_id();
        builder.add_instr( get_loc_( main_run_id ) );
        builder.add_instr( call0_() );
        builder.add_instr( return_() );
    }

    std::set< uint32_t > capture_root_locals( const fn_meta& fn )
    {
        std::set< uint32_t > captures;
        for ( const auto& insn : fn.body )
            if ( insn.kind == resolved_insn::kind_t::fn_ref )
                captures.insert( insn.target_fn_id );
        
        return captures;
    }

    void gen_fn( const fn_meta& fn )
    {
        using namespace qthu::as;
        const uint32_t fn_bc_idx = 1 + fn.id;
        ensure_patch( fn_bc_idx );

        // cpool + vars
        std::set< uint32_t > captures = capture_root_locals( fn ); 
        uint16_t closure_var_idx = 0;
        for ( uint32_t target_id : captures )
        {
            bc::closure_var cv;
            cv.var_name_atom = 0;
            cv.var_idx = static_cast< int32_t >( target_id );
            cv.closure_type = 0;
            cv.is_const = false;
            cv.is_lexical = false;
            cv.var_kind = 0;
            patches[ fn_bc_idx ].closure_vars.push_back( cv );
            fn_capture_idx[ fn.id ][ target_id ] = closure_var_idx++;
        }

        const uint16_t arg_count = static_cast< uint16_t >( fn.in.size() );
        const uint16_t local_count = static_cast< uint16_t >( fn.slot_size ); 
        const std::string fn_name = std::string( ir.st.name_of( fn.key.stru ) ) + "::" +
                                    std::string( ir.st.name_of( fn.key.op ) );

        builder.add_function( fn_name, arg_count, local_count, 256 );
        patches[ fn_bc_idx ].capture_all = true;

        // prologue
        for ( uint16_t a = 0; a < arg_count; ++a )
        {
            builder.add_instr( get_arg_( a ) );
            builder.add_instr( put_loc_( static_cast< int32_t >( fn.in_param_slots[ a ] ) ) );
        }

        // body
        uint32_t next_capture_local = fn.slot_size;
        uint32_t insn_uid = 0;
        for ( const lowered_insn& insn : fn.lowered )
        {
            ++insn_uid;
            switch ( insn.resolved.kind )
            {
                case resolved_insn::kind_t::builtin:
                    emit_builtin( insn, insn_uid );
                    break;

                case resolved_insn::kind_t::fn_ref:
                {
                    const uint32_t target_id = insn.resolved.target_fn_id;
                    auto it = fn_capture_idx[ fn.id ].find( target_id );
                    if ( it == fn_capture_idx[ fn.id ].end() )
                        throw std::runtime_error( "missing capture" );
                    
                    builder.add_instr( get_var_ref_( it->second ) );
                    builder.add_instr( put_loc_( insn.slots_out[ 0 ] ) );
                    break;
                }

                case resolved_insn::kind_t::fn_call:
                {
                    const uint16_t argc = static_cast< uint16_t >( insn.slots_in.size() - 1 );
                    builder.add_instr( get_loc_( insn.slots_in[ 0 ] ) );
                    for ( size_t i = 1; i < insn.slots_in.size(); ++ i )
                        builder.add_instr( get_loc_( insn.slots_in[ i ] ) );
                    
                    builder.add_instr( call_( argc ) );
                    builder.add_instr( put_loc_( insn.slots_out[ 0 ] ) );
                    break;
                }

                case resolved_insn::kind_t::fn_opt:
                {
                    emit_fn_opt( insn );
                    break;
                }

                case resolved_insn::kind_t::fn_join:
                {
                    emit_fn_join( insn );
                    break;
               }
            }
        }

        // epilogue
        if ( fn.out_param_slots.empty() )
        {
            builder.add_instr( undefined_() );
            builder.add_instr( return_() );
        }
        else
        {
            builder.add_instr( get_loc_( fn.out_param_slots.at( 0 ) ) );
            builder.add_instr( return_() );
        }
    }

    void gen_program()
    {
        fn_capture_idx.resize( ir.fns.size() );
        for ( const auto& fn : ir.fns )
            gen_fn( fn );
    }

    static void fill_captured_locals( bc::function_bytecode& f )
    {
        f.vardefs.resize( f.arg_count + f.local_count );
        f.var_ref_count = f.local_count;

        for ( uint16_t i = 0; i < f.local_count; ++i )
        {
            auto& vd = f.vardefs[ f.arg_count + i ];
            vd.var_name_atom = 0;
            vd.scope_next = -1;
            vd.var_ref_idx = i;
            vd.var_kind = 0;
            vd.is_const = false;
            vd.is_lexical = false;
            vd.is_captured = true;
            vd.has_scope = false;
        }
    }

    bc::program lower_to_bc()
    {
        if ( ir.fns.empty() )
            throw std::runtime_error( "no functions to compile" );

        gen_root();
        gen_program();
        std::cout << builder.print_asm();
        bc::program bc_prog = builder.build();

        for ( uint32_t i = 0; i < bc_prog.functions.size(); ++i )
        {
            ensure_patch( i );
            bc_prog.functions[ i ].cpool_funcs = patches[ i ].cpool_funcs;
            bc_prog.functions[ i ].closure_vars = patches[ i ].closure_vars;

            if ( patches[ i ].capture_all )
                fill_captured_locals( bc_prog.functions[ i ] );
        }

        return bc_prog;
    }
};

inline bc::program lower_to_bc( program& ir_prog )
{
    as::asmbuilder builder;
    codegen gen{ ir_prog, builder };
    return gen.lower_to_bc();
}

}
