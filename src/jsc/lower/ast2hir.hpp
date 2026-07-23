#pragma once

#include "../ir/hir.hpp"
#include "../sema/analysis.hpp"

namespace qthu::jsc::hir
{

// TODO: captures
template< typename... Args >
inline void error( Args&&... args )
{
    std::ostringstream out;
    ( out << ... << std::forward< Args >( args ) );
    throw std::runtime_error( out.str() );
}

struct lowering
{
    sema::analysis_result& sema;

    struct func_ctx
    {
        function fn;
    };

    expr_id append_expr( func_ctx& fc, hir::expr e )
    {
        expr_id id{ static_cast< uint32_t >( fc.fn.expressions.size() ) };
        fc.fn.expressions.push_back( std::move( e ) );
        return id;
    }

    stmt_id append_stmt( func_ctx& fc, hir::stmt s )
    {
        stmt_id id{ static_cast< uint32_t >( fc.fn.statements.size() ) };
        fc.fn.statements.push_back( std::move( s ) );
        return id;
    }
    
    expr_id lower_expr( func_ctx& fc, ast::expr& e )
    {
        hir::expr res{ .typ = type::jsvalue };

        if ( auto* il = std::get_if< ast::int_lit >( &e.data ) )
            res.data = hir::expr::int_lit{ .value = il->value };

        else if ( auto* bl = std::get_if< ast::bool_lit >( &e.data ) )
            res.data = hir::expr::bool_lit{ .value = bl->value };
        
        else if ( auto* id = std::get_if< ast::var >( &e.data ) )
            res.data = hir::expr::var{ .id = sema.identifier_bindings.at( &e ) };

        else if ( auto* u = std::get_if< ast::unary >( &e.data ) )
        {
            expr_id sub = lower_expr( fc, *u->sub );
            res.data = hir::expr::unary{ .op = u->op, .sub = sub };
        }

        else if ( auto* b = std::get_if< ast::binary >( &e.data ) )
        {
            expr_id left = lower_expr( fc, *b->left );
            expr_id right = lower_expr( fc, *b->right );
            res.data = hir::expr::binary{ .op = b->op, .left = left, .right = right};
        }

        else if ( auto* a = std::get_if< ast::assign >( &e.data ) )
        {
            expr_id value = lower_expr( fc, *a->value );
            res.data = hir::expr::assign{ .target = sema.assign_bindings.at( &e ), .value = value };
        }

        else if ( auto* c = std::get_if< ast::call >( &e.data ) )
        {
            sema::function_id fid = sema.direct_calls.at( &e );
            std::vector< expr_id > args;
            for ( auto& arg : c->args )
                args.push_back( lower_expr( fc, *arg ) );
            res.data = hir::expr::call{ .target = fid, .args = std::move( args ) };
        }

        return append_expr( fc, std::move( res ) );
    }

    expr_id lower_negated( func_ctx& fc, ast::expr& cond )
    {
        hir::expr res;
        res.data = hir::expr::unary{ .op = op_kind::NOT, .sub = lower_expr( fc, cond ) };
        return append_expr( fc, std::move( res ) );
    }

    stmt_id make_break( func_ctx& fc )
    {
        return append_stmt( fc, stmt{ .data = stmt::brk{} } );
    }

    stmt_id make_block( func_ctx& fc, std::vector< stmt_id > stmts )
    {
        return append_stmt( fc, stmt{ .data = stmt::block{ std::move( stmts ) } } );
    }

    stmt_id make_guard( func_ctx& fc, ast::expr& cond )
    {
        expr_id neg = lower_negated( fc, cond );
        stmt_id brk = make_break( fc );
        stmt::if_stmt guard{ .cond = neg, .then_branch = brk, .else_branch = std::nullopt };
        return append_stmt( fc, stmt{ .data = std::move( guard ) } );
    }

    std::optional< stmt_id > lower_stmt_opt( func_ctx& fc, ast::stmt& s, module& mod )
    {
        if ( std::get_if< ast::fn_declaration >( &s.data ) )
        {
            lower_function( s, fc.fn.id, mod );
            return std::nullopt;
        }
        return lower_stmt( fc, s, mod );
    }

    stmt_id lower_stmt( func_ctx& fc, ast::stmt& s, module& mod )
    {
        hir::stmt res{};

        if ( auto* b = std::get_if< ast::block >( &s.data ) )
        {
            std::vector< stmt_id > out;
            for ( auto& stmt : b->stmts )
                if ( auto id = lower_stmt_opt( fc, *stmt, mod ) )
                    out.push_back( *id );
            
            return make_block( fc, std::move( out ) );
        }

        else if ( auto* vd = std::get_if< ast::var_declaration >( &s.data ) )
        {
            std::vector< stmt_id > out;
            for ( auto& dec : vd->declarators )
            {
                std::optional< expr_id > value;
                if ( dec.init ) 
                    value = lower_expr( fc, dec.init.value() ) ;

                sema::binding_id bid = sema.declarator_bindings.at( &dec );
                hir::stmt::let_stmt ls{ .typ = type::jsvalue, .target = bid, .value = value };
                out.push_back( append_stmt( fc, stmt{ .data = std::move( ls ) } ) );
            }

            return out.size() == 1 ? out[ 0 ] : make_block( fc, std::move( out ) );
        }

        else if ( auto* r = std::get_if< ast::ret >( &s.data ) )
        {
            std::optional< expr_id > val;
            if ( r->value )
                val = lower_expr( fc, *r->value );
            return append_stmt( fc, stmt{ .data = stmt::ret_stmt{ val } } );
        }

        else if ( auto* i = std::get_if< ast::if_stmt >( &s.data ) )
        {
            expr_id cond = lower_expr( fc, i->cond );
            stmt_id then_b = lower_stmt( fc, *i->then_branch, mod );
            std::optional< stmt_id > else_b;
            if ( i->else_branch )
                else_b = lower_stmt( fc, *i->else_branch, mod );
            return append_stmt( fc, stmt{ .data = stmt::if_stmt{ cond, then_b, else_b } } );
        }

        else if ( auto* w = std::get_if< ast::while_stmt >( &s.data ) )
        {
            stmt_id guard = make_guard( fc, w->cond );
            stmt_id body = lower_stmt( fc, *w->body, mod );
            stmt_id loop_body = make_block( fc, { guard, body } );
            return append_stmt( fc, stmt{ .data = stmt::loop_stmt{ loop_body } } );
        }

        else if ( auto* dw = std::get_if< ast::do_while_stmt >( &s.data ) )
        {
            stmt_id body = lower_stmt( fc, *dw->body, mod );
            stmt_id guard = make_guard( fc, dw->cond );
            stmt_id loop_body = make_block( fc, { body, guard } );
            return append_stmt( fc, stmt{ .data = stmt::loop_stmt{ loop_body } } );
        }

        else if ( auto* fl = std::get_if< ast::for_stmt >( &s.data ) )
        {
            std::vector< stmt_id > outer;
            if ( fl->init )
                outer.push_back( lower_stmt( fc, *fl->init, mod ) );

            std::vector< stmt_id > body_stmts;
            if ( fl->cond )
                body_stmts.push_back( make_guard( fc, *fl->cond ) );

            body_stmts.push_back( lower_stmt( fc, *fl->body, mod ) );
            if ( fl->update )
                body_stmts.push_back( append_stmt( fc, stmt{ .data = stmt::expr_stmt{ lower_expr( fc, *fl->update ) } } ) );

            stmt_id loop_body = make_block( fc, std::move( body_stmts ) );
            outer.push_back( append_stmt( fc, stmt{ .data = stmt::loop_stmt{ loop_body } } ) );
            return make_block( fc, std::move( outer ) );
        }

        else if ( auto* es = std::get_if< ast::expr_stmt >( &s.data ) )
        {
            expr_id e = lower_expr( fc, es->value );
            return append_stmt( fc, stmt{ .data = stmt::expr_stmt{ e } } );
        }

        else if ( std::get_if< ast::brk >( &s.data ) )
            return append_stmt( fc, stmt{ .data = stmt::brk{} } );

        else if ( std::get_if< ast::cont >( &s.data ) )
            return append_stmt( fc, stmt{ .data = stmt::cont{} } );

        error( "unhandled statement kind in lowering" );
        return stmt_id{ .id = 69 };
    }

    void lower_function( ast::stmt& s, sema::function_id parent, module& mod )
    {
        func_ctx fc{};
        fc.fn.id = sema.stmt_functions.at( &s );
        fc.fn.lexical_parent = parent;

        std::vector < stmt_id > body_stmts;
        ast::fn_declaration& fd = std::get< ast::fn_declaration >( s.data );
        for ( auto& param : fd.params )
            fc.fn.parameters.push_back( sema.param_bindings.at( &param ) );
        
        for ( auto& sub : fd.body.stmts )
            if ( auto id = lower_stmt_opt( fc, *sub, mod ) )
                body_stmts.push_back( id.value() );
        
        fc.fn.body_root = make_block( fc, std::move( body_stmts ) );
        mod.functions.push_back( std::move( fc.fn ) );
    }

    hir::module lower_program( ast::program& ast )
    {
        hir::module mod{};
        func_ctx fc{};
        fc.fn.id = sema.global_function;
        fc.fn.lexical_parent = std::nullopt;

        std::vector< stmt_id > top;
        for ( auto& sub : ast.statements )
            if ( auto id = lower_stmt_opt( fc, *sub, mod ) )
                top.push_back( id.value() );
            
        fc.fn.body_root = make_block( fc, std::move( top ) );
        mod.script = std::move( fc.fn );
        return mod;
    }
};

}
