#pragma once

#include "../asm/asmbuilder.hpp"
#include "../bytecode/program.hpp"
#include "ir.hpp"

#include <cstdint>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace qthu::cthu
{

struct fn_patch
{
    std::vector< uint32_t > cpool_funcs;
    std::vector< bc::closure_var > closure_vars;
    bool capture_all = false;
};

struct codegen
{
    cthu::program& ir;
    as::asmbuilder& builder;
    std::vector< fn_patch > patches;
    std::vector< std::unordered_map< uint32_t, uint16_t > > fn_capture_idx;

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

    void emit_builtin( const lowered_insn& insn, uint32_t uid )
    {
        using namespace qthu::as;
        const std::string_view name = ir.st.name_of( insn.resolved.target );

        auto get1 = [&]() {
            builder.add_instr( get_loc_( static_cast< int32_t >( insn.slots_in.at( 0 ) ) ) );
        };
        auto get2 = [&]() {
            builder.add_instr( get_loc_( static_cast< int32_t >( insn.slots_in.at( 0 ) ) ) );
            builder.add_instr( get_loc_( static_cast< int32_t >( insn.slots_in.at( 1 ) ) ) );
        };
        auto put = [&]( uint32_t slot ) {
            builder.add_instr( put_loc_( static_cast< int32_t >( slot ) ) );
        };

        if ( name == "builtin_bv32dup" || name == "builtin_bv32fork" || name == "builtin_bool_dup" )
        {
            get1();
            builder.add_instr( dup_() );
            put( insn.slots_out.at( 0 ) );
            put( insn.slots_out.at( 1 ) );
            return;
        }

        if ( name == "builtin_bv32drop" )
            return;

        if ( name == "builtin_bv32move" )
        {
            get1();
            put( insn.slots_out.at( 0 ) );
            return;
        }

        if ( name.starts_with( "builtin_bv32cons_" ) )
        {
            const auto suffix = name.substr( std::string_view( "builtin_bv32cons_" ).size() );
            int32_t v = 0;
            try
            {
                v = std::stoi( std::string( suffix ) );
            }
            catch ( const std::exception& )
            {
                throw std::runtime_error( std::string( "invalid constant builtin: " ) +
                                          std::string( name ) );
            }
            builder.add_instr( push_i32_( v ) );
            put( insn.slots_out.at( 0 ) );
            return;
        }

        if ( name == "builtin_bv32add" )
        {
            get2();
            builder.add_instr( add_() );
            put( insn.slots_out.at( 0 ) );
            return;
        }
        if ( name == "builtin_bv32sub" )
        {
            get2();
            builder.add_instr( sub_() );
            put( insn.slots_out.at( 0 ) );
            return;
        }
        if ( name == "builtin_bv32mul" )
        {
            get2();
            builder.add_instr( mul_() );
            put( insn.slots_out.at( 0 ) );
            return;
        }
        if ( name == "builtin_bv32sdiv" || name == "builtin_bv32udiv" )
        {
            get2();
            builder.add_instr( div_() );
            put( insn.slots_out.at( 0 ) );
            return;
        }
        if ( name == "builtin_bv32srem" || name == "builtin_bv32urem" )
        {
            get2();
            builder.add_instr( mod_() );
            put( insn.slots_out.at( 0 ) );
            return;
        }
        if ( name == "builtin_bv32eq" )
        {
            get2();
            builder.add_instr( strict_eq_() );
            put( insn.slots_out.at( 0 ) );
            return;
        }
        if ( name == "builtin_bv32ne" )
        {
            get2();
            builder.add_instr( strict_eq_() );
            builder.add_instr( lnot_() );
            put( insn.slots_out.at( 0 ) );
            return;
        }
        if ( name == "builtin_bv32slt" || name == "builtin_bv32ult" )
        {
            get2();
            builder.add_instr( lt_() );
            put( insn.slots_out.at( 0 ) );
            return;
        }
        if ( name == "builtin_bv32sgt" || name == "builtin_bv32ugt" )
        {
            get2();
            builder.add_instr( gt_() );
            put( insn.slots_out.at( 0 ) );
            return;
        }
        if ( name == "builtin_bool_not" )
        {
            get1();
            builder.add_instr( lnot_() );
            put( insn.slots_out.at( 0 ) );
            return;
        }
        if ( name == "builtin_bool_assert" )
        {
            const std::string ok = "assert_ok_" + std::to_string( uid );
            get1();
            builder.add_instr( if_true_( ok ) );
            builder.add_instr( push_i32_( 1 ) );
            builder.add_instr( throw_() );
            builder.add_label( ok );
            return;
        }

        if ( name == "builtin_bv32join" )
        {
            const std::string L_defined = "join_x_defined_" + std::to_string( uid );
            const std::string L_end = "join_end_" + std::to_string( uid );

            builder.add_instr( get_loc_( static_cast< int32_t >( insn.slots_in.at( 0 ) ) ) );
            builder.add_instr( dup_() );
            builder.add_instr( is_undefined_() );
            builder.add_instr( if_false_( L_defined ) );
            builder.add_instr( drop_() );
            builder.add_instr( get_loc_( static_cast< int32_t >( insn.slots_in.at( 1 ) ) ) );
            builder.add_instr( goto_( L_end ) );
            builder.add_label( L_defined );
            builder.add_label( L_end );
            put( insn.slots_out.at( 0 ) );
            return;
        }

        throw std::runtime_error( std::string( "unimplemented builtin: " ) + std::string( name ) );
    }

    void gen_root()
    {
        using namespace qthu::as;

        builder.add_function( "main", 0, static_cast< uint16_t >( ir.fns.size() ), 256 );
        ensure_patch( 0 );
        patches[ 0 ].capture_all = true;
        patches[ 0 ].cpool_funcs.reserve( ir.fns.size() );

        for ( uint32_t id = 0; id < ir.fns.size(); ++id )
        {
            patches[ 0 ].cpool_funcs.push_back( 1 + id );
            builder.add_instr( fclosure_( static_cast< int32_t >( id ) ) );
            builder.add_instr( put_loc_( static_cast< int32_t >( id ) ) );
        }

        const uint32_t main_run_id = find_main_id();
        builder.add_instr( get_loc_( static_cast< int32_t >( main_run_id ) ) );
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

    uint32_t count_extra_locals( const fn_meta& fn )
    {
        uint32_t extra = 0;
        for ( const auto& insn : fn.lowered )
        {
            if ( insn.resolved.kind == resolved_insn::kind_t::fn_opt )
                extra += 2;
            else if ( insn.resolved.kind == resolved_insn::kind_t::fn_join )
                extra += 3;
        }
        
        return extra;
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

        uint32_t extra_local_count = count_extra_locals( fn );
        const uint16_t arg_count = static_cast< uint16_t >( fn.in.size() );
        const uint16_t local_count = static_cast< uint16_t >( fn.slot_size + extra_local_count );
        const std::string fn_name = std::string( ir.st.name_of( fn.key.stru ) ) + "::" +
                                    std::string( ir.st.name_of( fn.key.op ) );

        builder.add_function( fn_name, arg_count, local_count, 256 );
        patches[ fn_bc_idx ].capture_all = true;

        // prologue
        for ( uint16_t a = 0; a < arg_count; ++a )
        {
            builder.add_instr( get_arg_( a ) );
            builder.add_instr( put_loc_( static_cast< int32_t >( fn.in_param_slots.at( a ) ) ) );
        }

        // body
        uint32_t next_capture_local = fn.slot_size;
        uint32_t insn_uid = 0;
        for ( const auto& insn : fn.lowered )
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
                    
                    builder.add_instr( get_var_ref_( static_cast< int32_t >( it->second ) ) );
                    builder.add_instr( put_loc_( static_cast< int32_t >( insn.slots_out.at( 0 ) ) ) );
                    break;
                }

                case resolved_insn::kind_t::fn_call:
                {
                    const uint16_t argc = static_cast< uint16_t >( insn.slots_in.size() - 1 );
                    builder.add_instr( get_loc_( static_cast< int32_t >( insn.slots_in.at( 0 ) ) ) );
                    for ( size_t i = 1; i < insn.slots_in.size(); ++ i )
                        builder.add_instr( get_loc_( static_cast< int32_t >( insn.slots_in[ i ] ) ) );
                    
                    builder.add_instr( call_( argc ) );
                    builder.add_instr( put_loc_( static_cast< int32_t >( insn.slots_out.at( 0 ) ) ) );
                    break;
                }

                case resolved_insn::kind_t::fn_opt:
                {
                    // TODO
                }

                case resolved_insn::kind_t::fn_join:
                {
                    // TODO:
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
            builder.add_instr( get_loc_( static_cast< int32_t >( fn.out_param_slots.at( 0 ) ) ) );
            builder.add_instr( return_() );
        }
    }

    void gen_program()
    {
        fn_capture_idx.resize( ir.fns.size() );
        for ( const auto& fn : ir.fns )
            gen_fn( fn );
    }

    bc::program lower_to_bc()
    {
        if ( ir.fns.empty() )
            throw std::runtime_error( "no functions to compile" );

        gen_root();

        const uint32_t cthu_fn_count = static_cast< uint32_t >( ir.fns.size() );
        gen_program();
        // TODO
        return builder.build();
    }
};

inline bc::program lower_to_bc( program& ir_prog )
{
    as::asmbuilder builder;
    codegen gen{ ir_prog, builder };
    return gen.lower_to_bc();
}

} // namespace qthu::cthu
