#pragma once

#include "../ir/linear.hpp"
#include "../ir/hir.hpp"

namespace qthu::jsc
{

struct rename_env
{
    std::unordered_map< std::uint32_t, lin::value > current;
 
    lin::value& at( sema::binding_id bid ) { return current.at( bid.value ); }
    void set( sema::binding_id bid, lin::value v ) { current[ bid.value ] = v; }
    bool has( sema::binding_id bid ) const { return current.contains( bid.value ); }
};

struct value_namer
{
    uint32_t next = 0;
    lin::value fresh() { return lin::value{ .id = lin::value_id{ next++ }, .version = 0 }; };
};

struct hir_to_linear
{
    hir::function& fn;
    sema::analysis_result& sema;
    value_namer vn;

    // TODO: We will probably have to put constant into values, since then doing cons_ is much harder
    lin::argument lower_expr( rename_env& env, std::vector< lin::instr >& body, hir::expr_id eid )
    {
        const auto& node = fn.get( eid );

        if ( auto* lit = std::get_if< hir::expr::int_lit >( &node.data ) )
            return lin::constant{ lit->value };

        else if ( auto* lit = std::get_if< hir::expr::bool_lit >( &node.data ) )
            return lin::constant{ lit->value };

        else if ( auto* v = std::get_if< hir::expr::var >( &node.data ) )
            return env.at( v->id );

        else if ( auto* u = std::get_if< hir::expr::unary >( &node.data ) )
        {
            lin::argument sub = lower_expr( env, body, u->sub );
            lin::value target = vn.fresh();
            body.push_back( lin::instr{ lin::unary_data{ u->op, sub, target } } );
            return target;
        }

        else if ( auto* b = std::get_if< hir::expr::binary >( &node.data ) )
        {
            lin::argument l = lower_expr( env, body, b->left );
            lin::argument r = lower_expr( env, body, b->right );
            lin::value target = vn.fresh();
            body.push_back( lin::instr{ lin::binary_data{ b->op, l, r, target } } );
            return target;
        }

        else if ( auto* a = std::get_if< hir::expr::assign >( &node.data ) )
        {
            lin::argument val = lower_expr( env, body, a->value );

            lin::value v;
            if ( auto* existing = std::get_if< lin::value >( &val ) )
                v = *existing;
            else
            {
                v = vn.fresh();
                body.push_back( lin::instr{ lin::copy_data{ val, v } } );
            }

            env.set( a->target, v );
            return v;
        }

        else if ( auto* c = std::get_if< hir::expr::call >( &node.data ) )
        {
            assert( false && "unimplemented" );
            // std::vector< lin::argument > args;
            // args.reserve( c->args.size() );

            // for ( auto arg : c->args )
            //     args.push_back( lower_expr( env, body, arg ) );

            // lin::value target = vn.fresh();
            // body.push_back( lin::instr{ lin::call_data{ c->target, std::move( args ), target } } );
            // return target;
        }

        assert( false );
    }
    
    void lower_stmt( rename_env& env, std::vector< lin::instr >& body, hir::stmt_id sid )
    {
        const auto& node = fn.get( sid );

        if ( auto* st = std::get_if< hir::stmt::expr_stmt >( &node.data ) )
            lower_expr( env, body, st->expr );

        else if ( auto* st = std::get_if< hir::stmt::block >( &node.data ) )
        {
            for ( auto sub : st->stmts )
                lower_stmt( env, body, sub );
        }

        else if ( auto* st = std::get_if< hir::stmt::let_stmt >( &node.data ) )
        {
            lin::value v;
            if ( st->value )
            {
                lin::argument a = lower_expr( env, body, *st->value );

                if ( auto* value = std::get_if< lin::value >( &a ) )
                    v = *value;
                else
                {
                    v = vn.fresh();
                    body.push_back( lin::instr{ lin::copy_data{ a, v } } );
                }
            }
            else
                v = vn.fresh(); // TODO: what to do with uninitialized vars

            env.set( st->target, v );
        }

        else if ( auto* st = std::get_if< hir::stmt::ret_stmt >( &node.data ) )
        {
            std::optional< lin::argument > val;
            if ( st->value )
                val = lower_expr( env, body, *st->value );

            body.push_back( lin::instr{ lin::ret_data{ val } } );
        }

        else if ( auto* st = std::get_if< hir::stmt::if_stmt >( &node.data ) )
            assert( false );

        else if ( auto* st = std::get_if< hir::stmt::loop_stmt >( &node.data ) )
        {
            std::vector< lin::instr > loop_body;
            lower_stmt( env, loop_body, st->body );
            body.push_back( lin::instr{ lin::loop_data{ std::move( loop_body ) } } );
        }

        else if ( std::get_if< hir::stmt::brk >( &node.data ) )
            body.push_back( lin::instr{ lin::brk_data{} } );

        else if ( std::get_if< hir::stmt::cont >( &node.data ) )
            body.push_back( lin::instr{ lin::cont_data{} } );

        else
            assert( false );
    }

    lin::function lower_function()
    {
        lin::function res{ .name = fn.id, .body = {} };
        rename_env env;
        lower_stmt( env, res.body, fn.body_root );
        return res;
    }

};

inline lin::program lower_hir( hir::module& mod, sema::analysis_result& sema )
{
    lin::program prog{};
    hir_to_linear script_lowering{ mod.script, sema };
    prog.functions.push_back( script_lowering.lower_function() );

    for ( auto& fn : mod.functions )
    {
        hir_to_linear fl{ fn, sema };
        prog.functions.push_back( fl.lower_function() );
    }

    return prog;
}

}
