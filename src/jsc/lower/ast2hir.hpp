#pragma once

#include <iostream>

#include "../frontend/ast.hpp"
#include "../sema/analysis.hpp"
#include "../sema/types.hpp"
#include "../ir/hir.hpp"

namespace qthu::jsc::hir
{

struct ast_to_hir
{
    const sema::analysis_result& semantics;
    function& fn;

    ast_to_hir( const sema::analysis_result& semantics, function& fn )
        : semantics{ semantics }, fn{ fn } {}

    expr_id append_expr( expr e )
    {
        expr_id id{ static_cast< uint32_t >( fn.expressions.size() ) };
        fn.expressions.push_back( std::move( e ) );
        return id;
    }

    stmt_id append_stmt( stmt s )
    {
        stmt_id id{ static_cast< uint32_t >( fn.statements.size() ) };
        fn.statements.push_back( std::move( s ) );
        return id;
    }
    
    expr_id lower_expr( ast::expr& e )
    {
        hir::expr res{ .typ = type::jsvalue };
        switch ( e.cat )
        {
            case ast::expr::num_lit:
            {
                res.data = expr::int_lit{ .value = std::get< uint64_t >( e.val ) };
                break;
            }
            case ast::expr::bool_lit:
            {
                res.data = expr::bool_lit{ .value = std::get< bool >( e.val ) };
                break;
            }
            case ast::expr::identifier:
            {
                res.data = expr::var{ .id = var_id{ semantics.identifier_bindings.at( &e ).value } };
                break;
            }
            case ast::expr::unary:
            {
                res.data = expr::unary{ .op = e.op, .sub = lower_expr( e[ 0 ] ) };
                break;
            }
            // TODO: do we want to short-circuit relational operations -> eventually yes, but not rn..,
            // come back to this once we are implementing control flow...
            case ast::expr::binary:
            {
                res.data = expr::binary{ .op = e.op, .left = lower_expr( e[ 0 ] ), .right = lower_expr( e[ 1 ] ) };
                break;
            }
            case ast::expr::assign:
            {
                res.data = expr::assign{ .target = var_id{ semantics.assignment_targets.at( &e ).value },
                                         .value = lower_expr( e[ 1 ] ) };
                break;
            }
            case ast::expr::call:
            {
                auto call = expr::call{ .target = semantics.direct_calls.at( &e ) };
                for ( size_t i = 1; i < e.subs.size(); ++i )
                    call.args.push_back( lower_expr( e[ i ] ) );

                res.data = std::move( call );
                break;
            }
            default:
                break;
        }

        return append_expr( res );
    }

    stmt_id block( std::vector< ast::stmt >& statements )
    {
        stmt::block b{};
        for ( auto& s : statements )
            b.stmts.push_back( lower_stmt( s ) ) ;
        
        return append_stmt( stmt{ .data = std::move( b ) } );
    }

    stmt_id lower_stmt( ast::stmt& s )
    {
        switch ( s.cat )
        {
            case ast::stmt::expr_stmt:
                return append_stmt( hir::stmt{ .data = hir::stmt::expr_stmt{ lower_expr( *s.e ) } } );
            
            case ast::stmt::var_dclr:
            {
                if ( !s.vdecl.e )
                    throw std::runtime_error( "HIR lowering of uninitialized declarations is not implemented" );
                return append_stmt( hir::stmt{ .data = hir::stmt::let_stmt{
                    type::jsvalue,
                    hir::var_id{ semantics.declarations.at( &s.vdecl ).value },
                    lower_expr( *s.vdecl.e ),
                } } );
            }
            case ast::stmt::block:
                return block( s.subs );
            case ast::stmt::ret:
                if ( !s.e ) throw std::runtime_error( "HIR lowering of empty return is not implemented" );
                return append_stmt( hir::stmt{ .data = hir::stmt::ret_stmt{ lower_expr( *s.e ) } } );

            case ast::stmt::if_stmt:
            {
                const auto then_branch = lower_stmt( s[ 0 ] );
                std::vector< ast::stmt > empty_block{};
                const auto else_branch = s.subs.size() > 1 ? lower_stmt( s[ 1 ] ) : block( empty_block );
                return append_stmt( hir::stmt{ .data = hir::stmt::if_stmt{
                    lower_expr( *s.e ), then_branch, else_branch,
                } } );
            }
            case ast::stmt::while_stmt:
            case ast::stmt::for_stmt:
            case ast::stmt::do_while_stmt:
                throw std::runtime_error( "loop HIR lowering is not implemented" );
            case ast::stmt::brk:
                return append_stmt( hir::stmt{ .data = hir::stmt::brk{} } );
            case ast::stmt::cont:
                return append_stmt( hir::stmt{ .data = hir::stmt::cont{} } );
        }

        throw std::runtime_error( "unknown statement during HIR lowering" );
    }
};

inline hir::program lower2hir( ast::program& ast, sema::analysis_result& semantics )
{
    hir::program res{};
    // Create a toplevel function which will correspond to javascript toplevel
    for ( auto& toplevel : ast.toplevel_items )
    {
        auto* src_decl = std::get_if< ast::fn_decl >( &toplevel );
        if ( !src_decl )
            continue; // TODO: toplevel stmt lowering
        
        auto fn_id = semantics.function_declarations.at( src_decl );
        hir::function fn{ .name = fn_id };
        for ( auto& param : src_decl->params )
            fn.params.push_back( hir::var_id{ semantics.declarations.at( &param ).value } );
        
        ast_to_hir lowerer{ semantics, fn };
        fn.body_root = lowerer.block( src_decl->body );
        res.functions.push_back( std::move( fn ) );
    }

    return res;
}

}
