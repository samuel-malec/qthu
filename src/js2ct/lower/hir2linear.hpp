#pragma once

#include <queue>

#include "../ir/linear.hpp"
#include "../ir/hir.hpp"

namespace qthu::js2ct::lin
{

struct rename_env
{
    std::unordered_map< std::uint32_t, lin::value > scope;

    lin::value& at( sema::binding_id bid ) 
    {
        if ( auto it = scope.find( bid.value ); it != scope.end() )
            return it->second;

        assert( false && "value not found" );
    }

    void declare( sema::binding_id bid, lin::value v ) { scope[ bid.value ] = v; }

    void reassign( sema::binding_id bid, lin::value v )
    {
        if ( auto it = scope.find( bid.value ); it != scope.end() )
        { 
            it->second = v;
            return;
        }

        assert( false && "reassigning a binding that was never declared" );
    }

    bool has( sema::binding_id bid ) 
    {
        return scope.contains( bid.value );
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

    void clean_scope()
    {
        // TODO:
        // This should pop, and drop all the vars that are live still
    }

    lin::argument lower_expr( std::vector< lin::instr >& sink, rename_env& env, hir::expr_id eid )
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
        {
            lin::value fst = vn.fresh();
            lin::value snd = vn.fresh();
            sink.push_back( lin::instr{ lin::dup_data{ .arg1 = env.at( v->id ), .first = fst, .second = snd } } );
            env.reassign( v->id, snd );
            return fst;
        }

        else if ( auto* u = std::get_if< hir::expr::unary >( &node.data ) )
        {
            lin::argument sub = lower_expr( sink, env, u->sub );
            lin::value target = vn.fresh();
            sink.push_back( lin::instr{ lin::unary_data{ u->op, sub, target } } );
            return target;
        }

        else if ( auto* b = std::get_if< hir::expr::binary >( &node.data ) )
        {
            lin::argument l = lower_expr( sink, env, b->left );
            lin::argument r = lower_expr( sink, env, b->right );
            lin::value target = vn.fresh();
            sink.push_back( lin::instr{ lin::binary_data{ b->op, l, r, target } } );
            return target;
        }

        else if ( auto* a = std::get_if< hir::expr::assign >( &node.data ) )
        {
            lin::argument val = lower_expr( sink, env, a->value );
            auto target = env.at( a->target );
            sink.push_back( lin::instr{ lin::drop_data{ .target = target } } );
            sink.push_back( lin::instr{ lin::copy_data{ .arg1 = val, .target = target } } );
            return target;
        }

        else if ( auto* c = std::get_if< hir::expr::call >( &node.data ) )
        {
            std::vector< lin::argument > args;

            for ( auto arg : c->args )
                args.push_back( lower_expr( sink, env, arg ) );

            lin::value target = vn.fresh();
            sink.push_back( lin::instr{ lin::call_data{ c->target, std::move( args ), target } } );
            return target;
        }

        assert( false );
    }
    
    //  This doesn't work for the stacks that we return from the procedure 
    void cleanup_env( rename_env& env, std::vector< lin::instr >& sink )
    {
        for ( auto& [ bid, val ] : env.scope )
            sink.push_back( lin::instr{ .data = lin::drop_data{ .target = val } } );
    }
    
    void lower_stmt( std::vector< lin::instr >& sink, rename_env& env, hir::stmt_id sid )
    {
        const auto& node = fn.get( sid );
        if ( auto* es = std::get_if< hir::stmt::expr_stmt >( &node.data ) )
            lower_expr( sink, env, es->expr );

        else if ( auto* b = std::get_if< hir::stmt::block >( &node.data ) )
        {
            for ( auto sub : b->stmts )
                lower_stmt( sink, env, sub );
        }

        else if ( auto* ls = std::get_if< hir::stmt::let_stmt >( &node.data ) )
        {
            lin::value v;
            if ( ls->value )
            {
                lin::argument a = lower_expr( sink, env, *ls->value );
                if ( auto* value = std::get_if< lin::value >( &a ) )
                    v = *value;
                else
                {
                    v = vn.fresh();
                    sink.push_back( lin::instr{ lin::copy_data{ a, v } } );
                }
            }
            else
                v = vn.fresh();
            env.declare( ls->target, v );
        }

        else if ( auto* rs = std::get_if< hir::stmt::ret_stmt >( &node.data ) )
        {
            std::optional< lin::argument > val;
            if ( rs->value )
                val = lower_expr( sink, env, *rs->value );
            sink.push_back( lin::instr{ lin::ret_data{ val } } );
        }

        else if ( auto* ifd = std::get_if< hir::stmt::if_stmt >( &node.data ) )
        {
            auto condarg = lower_expr( sink, env, ifd->cond );
            
            std::vector< value > params{};
            for ( auto& [ bid, val ] : env.scope )
                params.push_back( val );

            std::vector< lin::instr > then_body;
            rename_env then_env = env;
            lower_stmt( then_body, then_env, ifd->then_branch );
            cleanup_env( then_env, then_body );

            std::vector< lin::instr > else_body;
            if ( ifd->else_branch )
            {
                rename_env else_env = env;
                lower_stmt( else_body, else_env, ifd->else_branch.value() );
                cleanup_env( else_env, else_body );
            }

            sink.push_back( lin::instr{ lin::if_data{ .cond = condarg, 
                           .then_body = std::move( then_body ),
                           .else_body = std::move( else_body ),
                           .params = std::move( params ) } } );
        }

        else if ( auto* st = std::get_if< hir::stmt::loop_stmt >( &node.data ) )
        {
            std::vector< sema::binding_id > live_bindings{};
            std::vector< value > params{};
            for ( auto& [ bid, val ] : env.scope )
            {
                live_bindings.push_back( sema::binding_id{ bid } );
                params.push_back( val );
            }

            // cond is lowered into its own instruction stream (cond_body),
            // separate from the loop's body, because it has to be
            // re-evaluated fresh on *every* entry to the loop -- both the
            // initial one and every recursive re-entry -- unlike if_stmt's
            // cond, which only runs once. At the cthu level, cond_body
            // becomes part of the self-recursive dispatcher function
            // (loop_N), while body becomes the "keep looping" branch
            // (loopbody_N) that tail-calls back into loop_N -- two SEPARATE
            // cthu functions, each taking only `params` as input. So
            // cond_body and body must each lower from their own independent
            // copy of `env`, starting fresh from the loop's entry bindings:
            // if they shared one threaded rename_env, body would end up
            // referencing temporaries cond's evaluation produced (e.g. the
            // "kept half" of a dup), which only exist inside loop_N's own
            // locals, not loopbody_N's -- a cross-function reference that
            // cthuc's slot allocator can't resolve.
            rename_env cond_env = env;
            std::vector< lin::instr > cond_body;
            lin::argument condarg = lower_expr( cond_body, cond_env, st->cond );

            // Whatever survives cond_body's dups (e.g. the "kept half" of a
            // binding cond read) -- this is what the post-cond dispatch call
            // has to use, since `params` itself (loop_N's *declared* input
            // names) may already be partly consumed by cond_body at that
            // point in loop_N's own instruction stream.
            std::vector< value > dispatch_args{};
            for ( auto& bid : live_bindings )
                dispatch_args.push_back( cond_env.at( bid ) );

            rename_env body_env = env;
            std::vector< lin::instr > body;
            lower_stmt( body, body_env, st->body );

            // Deliberately not calling cleanup_env here: it unconditionally
            // drops every scope entry (a known, separately-tracked bug --
            // see CLAUDE.md), which would drop the very params the
            // recursive tail-call at the cthu level needs to survive.
            std::vector< value > next_params{};
            for ( auto& bid : live_bindings )
                next_params.push_back( body_env.at( bid ) );

            // Every live binding gets a fresh id representing its value
            // once the loop actually exits at runtime, and env is
            // reassigned to it -- so code that follows the loop (e.g. a
            // `return` reading a loop-mutated variable) sees the loop's
            // result instead of the stale pre-loop value.
            std::vector< value > outputs{};
            for ( auto& bid : live_bindings )
            {
                value out = vn.fresh();
                outputs.push_back( out );
                env.reassign( bid, out );
            }

            sink.push_back( lin::instr{ lin::loop_data{ .cond_body = std::move( cond_body ),
                                                          .cond = condarg,
                                                          .dispatch_args = std::move( dispatch_args ),
                                                          .body = std::move( body ),
                                                          .params = std::move( params ),
                                                          .next_params = std::move( next_params ),
                                                          .outputs = std::move( outputs ) } } );
        }

        else if ( std::get_if< hir::stmt::brk >( &node.data ) )
            assert( false && "unimplemented" );

        else if ( std::get_if< hir::stmt::cont >( &node.data ) )
            assert( false && "unimplemented" );

        else
            assert( false && "unimplemented" );
    }

    lin::function lower_function()
    {
        lin::function res{ .name = fn.id, .body {} };
        rename_env env{};
        
        for ( auto& p : fn.parameters )
            env.declare( p, vn.fresh() ); 
        lower_stmt( res.body, env, fn.body_root );
        cleanup_env( env, res.body );

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
