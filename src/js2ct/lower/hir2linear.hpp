#pragma once

#include <queue>

#include "../ir/linear.hpp"
#include "../ir/hir.hpp"

namespace qthu::js2ct::lin
{

struct rename_env
{
    using scope = std::unordered_map< std::uint32_t, lin::value >;
    std::vector< scope > scopes;

    void push() { scopes.push_back( {} ); }
    void pop() { scopes.pop_back(); }
    
    scope& current() 
    { 
        assert( !scopes.empty() );
        return scopes.back();
    }

    lin::value& at( sema::binding_id bid ) 
    {
        for ( int i = scopes.size() - 1; i >= 0; --i )
            if ( auto it = scopes[ i ].find( bid.value ); it != scopes[ i ].end() )
                return it->second;
            
        assert( false && "value not found" );
    }

    void declare( sema::binding_id bid, lin::value v ) { scopes.back()[ bid.value ] = v; }

    void reassign( sema::binding_id bid, lin::value v )
    {
        for ( int i = (int)scopes.size() - 1; i >= 0; --i )
            if ( auto it = scopes[ i ].find( bid.value ); it != scopes[ i ].end() )
            { 
                it->second = v;
                return;
            }

        assert( false && "reassigning a binding that was never declared" );
    }

    bool has( sema::binding_id bid ) 
    {
        for ( int i = scopes.size() - 1; i >= 0; --i )
        return scopes[ i ].contains( bid.value );
    }
};

struct value_namer
{
    uint32_t next = 0;
    lin::value fresh() { return lin::value{ .id = next++ }; };
};

struct hir_to_linear
{
    hir::function& fn;
    sema::analysis_result& sema;
    value_namer vn;
    rename_env env{};

    lin::argument lower_expr( std::vector< lin::instr >& sink, hir::expr_id eid )
    {
        const auto& node = fn.get( eid );

        if ( auto* lit = std::get_if< hir::expr::int_lit >( &node.data ) )
        {
            lin::value target = vn.fresh();
            sink.push_back( lin::instr{ lin::cons_data{ .c = lit->value, .target = target } } );
            return target;
        }

        else if ( auto* lit = std::get_if< hir::expr::bool_lit >( &node.data ) )
        {
            lin::value target = vn.fresh();
            sink.push_back( lin::instr{ lin::cons_data{ .c = lit->value, .target = target } } );
            return target;
        }

        else if ( auto* v = std::get_if< hir::expr::var >( &node.data ) )
            return env.at( v->id );

        else if ( auto* u = std::get_if< hir::expr::unary >( &node.data ) )
        {
            lin::argument sub = lower_expr( sink, u->sub );
            lin::value target = vn.fresh();
            sink.push_back( lin::instr{ lin::unary_data{ u->op, sub, target } } );
            return target;
        }

        else if ( auto* b = std::get_if< hir::expr::binary >( &node.data ) )
        {
            lin::argument l = lower_expr( sink, b->left );
            lin::argument r = lower_expr( sink, b->right );
            lin::value target = vn.fresh();
            sink.push_back( lin::instr{ lin::binary_data{ b->op, l, r, target } } );
            return target;
        }

        else if ( auto* a = std::get_if< hir::expr::assign >( &node.data ) )
        {
            lin::argument val = lower_expr( sink, a->value );
            lin::value v;
            if ( auto* existing = std::get_if< lin::value >( &val ) )
                v = *existing;
            else
            {
                v = vn.fresh();
                sink.push_back( lin::instr{ lin::copy_data{ val, v } } );
            }

            env.reassign( a->target, v );
            return v;
        }

        else if ( auto* c = std::get_if< hir::expr::call >( &node.data ) )
        {
            assert( false && "unimplemented" );
            std::vector< lin::argument > args;

            for ( auto arg : c->args )
                args.push_back( lower_expr( sink, arg ) );

            lin::value target = vn.fresh();
            sink.push_back( lin::instr{ lin::call_data{ c->target, std::move( args ), target } } );
            return target;
        }

        assert( false );
    }
    
    void lower_stmt( std::vector< lin::instr >& sink, hir::stmt_id sid )
    {
        const auto& node = fn.get( sid );
        if ( auto* st = std::get_if< hir::stmt::expr_stmt >( &node.data ) )
            lower_expr( sink, st->expr );

        else if ( auto* st = std::get_if< hir::stmt::block >( &node.data ) )
        {
            env.push();
            for ( auto sub : st->stmts )
                lower_stmt( sink, sub );
            env.pop();
        }

        else if ( auto* st = std::get_if< hir::stmt::let_stmt >( &node.data ) )
        {
            lin::value v;
            if ( st->value )
            {
                lin::argument a = lower_expr( sink, *st->value );

                if ( auto* value = std::get_if< lin::value >( &a ) )
                    v = *value;
                else
                {
                    v = vn.fresh();
                    sink.push_back( lin::instr{ lin::copy_data{ a, v } } );
                }
            }
            else
                v = vn.fresh(); // TODO: what to do with uninitialized vars

            env.declare( st->target, v );
        }

        else if ( auto* st = std::get_if< hir::stmt::ret_stmt >( &node.data ) )
        {
            std::optional< lin::argument > val;
            if ( st->value )
                val = lower_expr( sink, *st->value );

            sink.push_back( lin::instr{ lin::ret_data{ val } } );
        }

        else if ( auto* st = std::get_if< hir::stmt::if_stmt >( &node.data ) )
        {
            auto cond_arg = lower_expr( sink, st->cond );

            rename_env then_env = env;
            rename_env else_env = env;

            std::vector< lin::instr > then_body;
            std::swap( env, then_env );
            env.push();
            lower_stmt( then_body, st->then_branch );
            env.pop();
            std::swap( env, then_env );

            std::vector< lin::instr > else_body;
            if ( st->else_branch )
            {
                std::swap( env, else_env );
                env.push();
                lower_stmt( else_body, st->else_branch.value() );
                env.pop();
                std::swap( env, else_env );
            }
            // TODO: merge values from both branches
        }

        else if ( auto* st = std::get_if< hir::stmt::loop_stmt >( &node.data ) )
        {
            env.push();
            std::vector< lin::instr > loop_body;
            lower_stmt( loop_body, st->body );
            env.pop();
            sink.push_back( lin::instr{ lin::loop_data{ std::move( loop_body ) } } );
        }

        else if ( std::get_if< hir::stmt::brk >( &node.data ) )
            assert( false && "unimplemented" );

        else if ( std::get_if< hir::stmt::cont >( &node.data ) )
            assert( false && "unimplemented" );

        else
            assert( false && "unimplemented" );
    }

    struct info
    {
        int total;
        int remaining;
        lin::value current;
    };

    void compute_scope_use_count( std::unordered_map< uint32_t, info >& res, std::vector< lin::instr >& ins )
    {
        for ( auto& i : ins )
        {
            i.for_each_use( [ & ]( lin::value& v ) { res[ v.id ].total++; } );
            if ( auto* id = std::get_if< lin::if_data >( &i.data ) )
            {   
                std::set< uint32_t > live;
                collect_live_vars( id->then_body, live );
                collect_live_vars( id->else_body, live );
                std::set< uint32_t > seen;

                for ( auto& v : live )
                    if ( seen.insert( v ).second )
                        res[ v ].total++;
            }
            else if ( auto* ld = std::get_if< lin::loop_data >( &i.data ) )
            {
                std::set< uint32_t > live;
                collect_live_vars( ld->body, live );
                for ( auto& v : live )
                    res[ v ].total++;
            }
        }
    }

    void linearize( std::vector< lin::instr >& sink, std::vector< lin::instr >& ins, value_namer& vn )
    {
        std::unordered_map< uint32_t, info > val_info{};
        compute_scope_use_count( val_info, ins );

        for ( auto& [ val, info ] : val_info )
        {
            info.remaining = info.total;
            info.current = lin::value{ .id = val };
        }

        for ( auto& i : ins )
        {
            i.for_each_use( [ & ]( lin::value& v )
            {
                auto& info = val_info[ v.id ];
                if ( info.remaining == 1 )
                {
                    v.id = info.current.id;
                    info.remaining--;
                    return;
                }

                if ( info.remaining > 1 )
                {
                    auto v1 = vn.fresh();
                    auto v2 = vn.fresh();
                    sink.push_back( lin::instr{ lin::dup_data{ .arg1 = info.current, .first = v1, .second = v2 } } );
                    v.id = v1.id;
                    info.current = v2;
                }

                info.remaining--;
            });
            if ( auto* id = std::get_if< lin::if_data >( &i.data ) )
            {
                lin::if_data linear_if{};
                linear_if.cond = std::move( id->cond );
                linearize( linear_if.then_body, id->then_body, vn );
                linearize( linear_if.else_body, id->else_body, vn );
                sink.push_back( lin::instr{ .data = std::move( linear_if ) } );
            }
            else if ( auto* ld = std::get_if< lin::loop_data>( &i.data ) )
            {
                lin::loop_data linear_loop{};
                linearize( linear_loop.body, ld->body, vn );
                sink.push_back( lin::instr{ .data = std::move( linear_loop ) } );
            }
            else
                sink.push_back( std::move( i ) );
        }
    }

    lin::function lower_function()
    {
        lin::function res{ .name = fn.id, .body {} };
        env.push();
        for ( auto& p : fn.parameters )
            env.declare( p, vn.fresh() );
        
        std::vector< lin::instr > sink;
        lower_stmt( sink, fn.body_root );
        env.pop();
        
        res.body = std::move( sink );
        // linearize( res.body, sink, vn );
        return res;
    }
};

struct lowerer
{
    sema::analysis_result& sema;

    lin::program lower( hir::module& mod )
    {
        lin::program prog{};
        hir_to_linear script_lowering{ mod.script, sema };
        prog.functions.push_back( std::move( script_lowering.lower_function() ) );

        for ( auto& fn : mod.functions )
        {
            hir_to_linear fl{ fn, sema };
            prog.functions.push_back( std::move( fl.lower_function() ) );
        }
        return prog;
    } 
};

}
