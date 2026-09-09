#pragma once

#include <queue>

#include "../ir/linear.hpp"
#include "../ir/hir.hpp"

namespace qthu::js2ct::lin
{

struct rename_env
{
    std::unordered_map< std::uint32_t, value > scope;

    value& at( sema::binding_id bid )
    {
        if ( auto it = scope.find( bid.value ); it != scope.end() )
            return it->second;

        assert( false && "value not found" );
    }

    void declare( sema::binding_id bid, value v )
    {
        scope[ bid.value ] = v;
    }

    void reassign( sema::binding_id bid, value v )
    {
        if ( auto it = scope.find( bid.value ); it != scope.end() )
        {
            it->second = v;
            return;
        }

        assert( false && "reassigning a binding that was never declared" );
    }

    bool has( sema::binding_id bid ) const
    {
        return scope.contains( bid.value );
    }
};

struct value_namer
{
    uint32_t next = 0;

    value fresh()
    {
        return lin::value{ .id = next++ };
    };
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

    argument lower_expr( std::vector< instr >& sink, rename_env& env, hir::expr_id eid )
    {
        const auto& node = fn.get( eid );
        if ( auto* lit = std::get_if< hir::expr::int_lit >( &node.data ) )
        {
            value target = vn.fresh();
            sink.push_back( instr{ cons_data{ .c = lit->value, .target = target } } );
            return target;
        }
        if ( auto* lit = std::get_if< hir::expr::bool_lit >( &node.data ) )
        {
            value target = vn.fresh();
            sink.push_back( instr{ lin::cons_data{ .c = lit->value, .target = target } } );
            return target;
        }
        if ( auto* v = std::get_if< hir::expr::var >( &node.data ) )
        {
            value fst = vn.fresh();
            value snd = vn.fresh();
            sink.push_back(
                instr{ dup_data{ .arg1 = env.at( v->id ), .first = fst, .second = snd } } );
            env.reassign( v->id, snd );
            return fst;
        }
        if ( auto* u = std::get_if< hir::expr::unary >( &node.data ) )
        {
            argument sub = lower_expr( sink, env, u->sub );
            value target = vn.fresh();
            sink.push_back( instr{ unary_data{ u->op, sub, target } } );
            return target;
        }
        if ( auto* b = std::get_if< hir::expr::binary >( &node.data ) )
        {
            argument l = lower_expr( sink, env, b->left );
            argument r = lower_expr( sink, env, b->right );
            value target = vn.fresh();
            sink.push_back( instr{ binary_data{ b->op, l, r, target } } );
            return target;
        }
        if ( auto* a = std::get_if< hir::expr::assign >( &node.data ) )
        {
            argument val = lower_expr( sink, env, a->value );
            auto target = env.at( a->target );
            sink.push_back( instr{ drop_data{ .target = target } } );
            sink.push_back( instr{ copy_data{ .arg1 = val, .target = target } } );
            return target;
        }
        if ( auto* c = std::get_if< hir::expr::call >( &node.data ) )
        {
            std::vector< argument > args;

            for ( auto arg : c->args )
                args.push_back( lower_expr( sink, env, arg ) );

            value target = vn.fresh();
            sink.push_back( instr{ call_data{ c->target, std::move( args ), target } } );
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
            value v;
            if ( ls->value )
            {
                argument a = lower_expr( sink, env, *ls->value );
                if ( auto* value = std::get_if< lin::value >( &a ) )
                    v = *value;
                else
                {
                    v = vn.fresh();
                    sink.push_back( instr{ copy_data{ a, v } } );
                }
            }
            else
                v = vn.fresh();
            env.declare( ls->target, v );
        }

        else if ( auto* rs = std::get_if< hir::stmt::ret_stmt >( &node.data ) )
        {
            std::optional< argument > val;
            if ( rs->value )
                val = lower_expr( sink, env, *rs->value );
            sink.push_back( instr{ ret_data{ val } } );
        }

        else if ( auto* ifd = std::get_if< hir::stmt::if_stmt >( &node.data ) )
        {
            auto condarg = lower_expr( sink, env, ifd->cond );

            std::vector< value > params{};
            for ( auto& [ bid, val ] : env.scope )
                params.push_back( val );

            std::vector< instr > then_body;
            rename_env then_env = env;
            lower_stmt( then_body, then_env, ifd->then_branch );
            cleanup_env( then_env, then_body );

            std::vector< instr > else_body;
            if ( ifd->else_branch )
            {
                rename_env else_env = env;
                lower_stmt( else_body, else_env, ifd->else_branch.value() );
                cleanup_env( else_env, else_body );
            }

            sink.push_back( instr{ if_data{ .cond = condarg,
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

            rename_env cond_env = env;
            std::vector< instr > cond_body;
            argument condarg = lower_expr( cond_body, cond_env, st->cond );


            std::vector< value > dispatch_args{};
            for ( auto& bid : live_bindings )
                dispatch_args.push_back( cond_env.at( bid ) );

            rename_env body_env = env;
            std::vector< instr > body;
            lower_stmt( body, body_env, st->body );

            std::vector< value > next_params{};
            for ( auto& bid : live_bindings )
                next_params.push_back( body_env.at( bid ) );

            std::vector< value > outputs{};
            for ( auto& bid : live_bindings )
            {
                value out = vn.fresh();
                outputs.push_back( out );
                env.reassign( bid, out );
            }

            sink.push_back( instr{ loop_data{ .cond_body = std::move( cond_body ),
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

    function lower_function()
    {
        function res{ .name = fn.id, .body{} };
        rename_env env{};

        for ( auto& p : fn.parameters )
        {
            value v = vn.fresh();
            env.declare( p, v );
            res.params.push_back( v );
        }
        lower_stmt( res.body, env, fn.body_root );
        cleanup_env( env, res.body );

        return res;
    }
};

struct lowerer
{
    sema::analysis_result& sema;

    program lower( hir::module& mod )
    {
        program prog{};
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

} // namespace qthu::js2ct::lin
