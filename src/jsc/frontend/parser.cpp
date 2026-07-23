#include <iostream>
#include <charconv>

#include "parser.hpp"

namespace qthu::jsc
{
    using cat = token::cat_t;
    
    ast::expr parser::make_increment_expr( ast::expr target, bool is_incr )
    {
        assert( false && "TODO: implement this inside HIR" );
    }

    ast::expr parser::make_compound_assign( ast::expr lhs, std::string_view compound_op, ast::expr rhs )
    {
        auto* v = std::get_if< ast::var >( &lhs.data );
        if ( !v )
            error( "Left-hand side of compound assignment must be an identifier" );

        ast::var target_var = *v;
        location loc = lhs.loc;
        std::string_view base_op = compound_op.substr( 0, compound_op.size() - 1 );

        ast::expr bin = make_binary( std::move( lhs ), op_kind_from_str( base_op ), std::move( rhs ) );

        return ast::expr{
            .loc = loc,
            .data = ast::assign{
                .target = target_var,
                .value = make_expr_ptr( std::move( bin ) ),
            }
        };
    }

    std::optional< ast::expr > parser::parse_primary()
    {
        if ( match( cat::punct, "(" ) )
        {
            fetch();
            auto e = parse_expr();
            require( cat::punct, ")" );
            return e;
        }

        if ( match( cat::number ) )
        {
            auto tok = fetch();
            uint64_t n{};
            auto [ p, ec ] = std::from_chars( tok.data.data(), tok.data.data() + tok.data.size(), n );
            if ( ec != std::errc() || p != tok.data.data() + tok.data.size() )
                error( tok, "Invalid numeric literal" );

            return ast::expr{ .loc = tok.loc, .data = ast::int_lit{ n } };
        }

        if ( auto t = match_any( cat::keyword, "true", "false" ) )
        {
            fetch();
            bool value = t->data == "true";
            return ast::expr{ .loc = t->loc, .data = ast::bool_lit{ value } };
        }

        if ( match( cat::ident ) )
        {
            auto tok = fetch();
            return ast::expr{ .loc = tok.loc, .data = ast::var{ tok.data } };
        }

        return {};
    }

    std::optional< ast::expr > parser::parse_postfix()
    {
        auto e = parse_primary();
        if ( !e )
            return {};

        while ( true )
        {
            if ( match( cat::punct, "(" ) )
            {
                location loc = e->loc;
                fetch();

                ast::call c{ .callee = make_expr_ptr( std::move( e.value() ) ), .args = {} };

                if ( !match( cat::punct, ")" ) )
                {
                    while ( true )
                    {
                        auto arg = parse_expr();
                        if ( !arg )
                            error( "Expected function argument" );

                        c.args.push_back( make_expr_ptr( std::move( arg.value() ) ) );

                        if ( !match( cat::punct, "," ) )
                            break;
                        fetch();
                    }
                }

                require( cat::punct, ")" );
                e = ast::expr{ .loc = loc, .data = std::move( c ) };
                continue;
            }

            if ( auto t = match_any( cat::punct, "++", "--" ) )
            {
                fetch();
                e = make_increment_expr( std::move( e.value() ), t->data == "++" );
                continue;
            }

            break;
        }

        return e;
    }

    std::optional< ast::expr > parser::parse_unary()
    {
        if ( auto t = match_any( cat::punct, "!", "-", "+" ) )
        {
            fetch();
            auto rhs = parse_unary();
            if ( !rhs )
                error( "Expected unary operand" );

            return ast::expr{
                .loc = t->loc,
                .data = ast::unary{
                    .op = op_kind_from_str( t->data ),
                    .sub = make_expr_ptr( std::move( rhs.value() ) ),
                }
            };
        }

        if ( auto t = match_any( cat::punct, "++", "--" ) )
        {
            fetch();
            auto rhs = parse_unary();
            if ( !rhs )
                error( "Expected operand after ++/--" );

            return make_increment_expr( std::move( rhs.value() ), t->data == "++" );
        }

        return parse_postfix();
    }

    std::optional< ast::expr > parser::parse_factor()
    {
        auto e = parse_unary();
        if ( !e )
            return {};

        while ( auto t = match_any( cat::punct, "/", "*", "%" ) )
        {
            fetch();
            auto rhs = parse_unary();
            if ( !rhs )
                error( "Expected rhs for multiplicative expression" );

            e = make_binary( std::move( e.value() ), op_kind_from_str( t->data ), std::move( rhs.value() ) );
        }

        return e;
    }

    std::optional< ast::expr > parser::parse_term()
    {
        auto e = parse_factor();
        if ( !e )
            return {};

        while ( auto t = match_any( cat::punct, "+", "-" ) )
        {
            fetch();
            auto rhs = parse_factor();
            if ( !rhs )
                error( "Expected rhs for additive expression" );

            e = make_binary( std::move( e.value() ), op_kind_from_str( t->data ), std::move( rhs.value() ) );
        }

        return e;
    }

    std::optional< ast::expr > parser::parse_shift()
    {
        auto e = parse_term();
        if ( !e )
            return {};

        while ( auto t = match_any( cat::punct, ">>", "<<" ) )
        {
            fetch();
            auto rhs = parse_term();
            if ( !rhs )
                error( "Expected rhs for shift expression" );

            e = make_binary( std::move( e.value() ), op_kind_from_str( t->data ), std::move( rhs.value() ) );
        }

        return e;
    }

    // TODO: distinguish between binary and relational nodes in the ast ( or postpone it to HIR lowering... )
    std::optional< ast::expr > parser::parse_comparison()
    {
        auto e = parse_shift();
        if ( !e )
            return {};

        while ( auto t = match_any( cat::punct, "<", "<=", ">", ">=" ) )
        {
            fetch();
            auto rhs = parse_shift();
            if ( !rhs )
                error( "Expected rhs for comparison expression" );

            e = make_binary( std::move( e.value() ), op_kind_from_str( t->data ), std::move( rhs.value() ) );
        }

        return e;
    }

    std::optional< ast::expr > parser::parse_equality()
    {
        auto e = parse_comparison();
        if ( !e )
            return {};

        while ( auto t = match_any( cat::punct, "==", "===", "!=", "!==" ) )
        {
            fetch();
            auto rhs = parse_comparison();
            if ( !rhs )
                error( "Expected rhs for equality expression" );

            e = make_binary( std::move( e.value() ), op_kind_from_str( t->data ), std::move( rhs.value() ) );
        }

        return e;
    }

    std::optional< ast::expr > parser::parse_and()
    {
        auto e = parse_equality();
        if ( !e )
            return {};

        while ( auto t = match_any( cat::punct, "&&" ) )
        {
            fetch();
            auto rhs = parse_equality();
            if ( !rhs )
                error( "Expected rhs for logical-and expression" );

            e = make_binary( std::move( e.value() ), op_kind_from_str( t->data ), std::move( rhs.value() ) );
        }

        return e;
    }

    std::optional< ast::expr > parser::parse_or()
    {
        auto e = parse_and();
        if ( !e )
            return {};

        while ( auto t = match_any( cat::punct, "||" ) )
        {
            fetch();
            auto rhs = parse_and();
            if ( !rhs )
                error( "Expected rhs for logical-or expression" );

            e = make_binary( std::move( e.value() ), op_kind_from_str( t->data ), std::move( rhs.value() ) );
        }

        return e;
    }

    std::optional< ast::expr > parser::parse_assignment()
    {
        auto e = parse_or();
        if ( !e )
            return {};

        if ( auto t = match_any( cat::punct, "=", "+=", "-=", "*=", "/=", "%=" ) )
        {
            fetch();
            auto rhs = parse_assignment();
            if ( !rhs )
                error( "Expected rhs for assignment expression" );

            if ( t->data == "=" )
            {
                auto* target_var = std::get_if< ast::var >( &e->data );
                if ( !target_var )
                    error( "Left-hand side of assignment must be an identifier" );

                location loc = e->loc;
                e = ast::expr{
                    .loc = loc,
                    .data = ast::assign{
                        .target = *target_var,
                        .value = make_expr_ptr( std::move( rhs.value() ) ),
                    }
                };
            }
            else
                e = make_compound_assign( std::move( e.value() ), t->data, std::move( rhs.value() ) );
        }

        return e;
    }

    std::optional< ast::expr > parser::parse_expr()
    {
        return parse_assignment();
    }

    std::optional< ast::stmt > parser::parse_expr_stmt()
    {
        auto e = parse_expr();
        if ( !e )
            return {};

        location loc = e->loc;
        require( cat::punct, ";" );
        return ast::stmt{ .loc = loc, .data = ast::expr_stmt{ .value = std::move( e.value() ) } };
    }

    std::optional< ast::stmt > parser::parse_block()
    {
        if ( !match( cat::punct, "{" ) )
            return {};

        location loc = peek().loc;
        fetch();

        ast::block blk{ .stmts = {} };

        while ( !match( cat::punct, "}" ) )
        {
            auto s = parse_stmt();
            if ( !s )
                error( "Parsing statement inside a block" );

            blk.stmts.push_back( std::make_unique< stmt >( std::move( s.value() ) ) );
        }

        fetch();
        return ast::stmt{ .data = std::move( blk ) };
    }

    std::optional< ast::fn_declaration > parser::parse_fn_decl()
    {
        if ( !match( cat::keyword, "function" ) )
            return {};

        location loc = peek().loc;
        fetch();
        auto id = require( cat::ident );
        require( cat::punct, "(" );

        fn_declaration res{ .name = id.data, .params = parse_param_list(), .body = {} };
        require( cat::punct, ")" );
        require( cat::punct, "{" );

        ast::block body{ .stmts = {} };
        while ( !match( cat::punct, "}" ) )
        {
            auto s = parse_stmt();
            if ( !s )
                error( "Parsing statement in function", id );
            body.stmts.push_back( std::make_unique< stmt > ( std::move( s.value() ) ) );
        }
        fetch();

        res.body = std::move( body );
        return res;
    }

    std::optional< ast::stmt > parser::parse_fn_decl_stmt()
    {
        auto f = parse_fn_decl();
        if ( !f )
            return {};

        return ast::stmt{ .data = std::move( f.value() ) };
    }

    std::optional< ast::stmt > parser::parse_if()
    {
        if ( !match( cat::keyword, "if" ) )
            return {};

        location loc = peek().loc;
        fetch();
        require( cat::punct, "(" );

        auto cond = parse_expr();
        if ( !cond )
            error( "Expected condition after if" );

        require( cat::punct, ")" );
        auto then_body = parse_stmt();
        if ( !then_body )
            error( "Empty body inside if block" );

        ast::if_stmt res{
            .cond = std::move( cond.value() ),
            .then_branch = make_stmt_ptr( std::move( then_body.value() ) ),
            .else_branch = nullptr,
        };

        if ( !match( cat::keyword, "else" ) )
            return ast::stmt{ .data = std::move( res ) };

        fetch();
        auto else_body = parse_stmt();
        if ( !else_body )
            error( "Empty else body" );

        res.else_branch = make_stmt_ptr( std::move( else_body.value() ) );
        return ast::stmt{ .loc = loc, .data = std::move( res ) };
    }

    std::optional< ast::stmt > parser::parse_ret()
    {
        if ( !match( cat::keyword, "return" ) )
            return {};

        location loc = peek().loc;
        fetch();

        if ( match( cat::punct, ";" ) )
        {
            fetch();
            return ast::stmt{ .data = ast::ret{ .value = std::nullopt } };
        }

        auto e = parse_expr();
        if ( !e )
            error( "Expected expression" );

        require( cat::punct, ";" );
        return ast::stmt{ .loc = loc, .data = ast::ret{ .value = std::move( e.value() ) } };
    }

    std::optional< ast::stmt > parser::parse_control_stmt()
    {
        if ( match( cat::keyword, "break" ) )
        {
            location loc = peek().loc;
            fetch();
            require( cat::punct, ";" );
            return ast::stmt{ .loc = loc, .data = ast::brk{} };
        }

        if ( match( cat::keyword, "continue" ) )
        {
            location loc = peek().loc;
            fetch();
            require( cat::punct, ";" );
            return ast::stmt{ .loc = loc, .data = ast::cont{} };
        }

        return {};
    }

    std::optional< ast::stmt > parser::parse_for()
    {
        if ( !match( cat::keyword, "for" ) )
            return {};

        location loc = peek().loc;
        fetch();
        require( cat::punct, "(" );

        ast::stmt_ptr init = nullptr;

        if ( !match( cat::punct, ";" ) )
        {
            if ( auto v = parse_var_decl() )
                init = make_stmt_ptr( std::move( v.value() ) );
            else
            {
                auto ie = parse_expr();
                if ( !ie )
                    error( "Invalid for loop initializer" );

                location iloc = ie->loc;
                require( cat::punct, ";" );
                init = make_stmt_ptr( ast::stmt{
                    .data = ast::expr_stmt{ .value = std::move( ie.value() ) }
                } );
            }
        }
        else
            fetch();

        ast::expr_ptr cond = nullptr;
        if ( !match( cat::punct, ";" ) )
        {
            auto ce = parse_expr();
            if ( !ce )
                error( "Invalid for loop condition" );
            cond = make_expr_ptr( std::move( ce.value() ) );
        }
        require( cat::punct, ";" );

        ast::expr_ptr update = nullptr;
        if ( !match( cat::punct, ")" ) )
        {
            auto ue = parse_expr();
            if ( !ue )
                error( "Invalid for loop update" );
            update = make_expr_ptr( std::move( ue.value() ) );
        }
        require( cat::punct, ")" );

        auto body = parse_stmt();
        if ( !body )
            error( "Empty body in for loop" );

        return ast::stmt{
            .loc = loc,
            .data = ast::for_stmt{
                .init = std::move( init ),
                .cond = std::move( cond ),
                .update = std::move( update ),
                .body = make_stmt_ptr( std::move( body.value() ) ),
            }
        };
    }

    std::optional< ast::stmt > parser::parse_while()
    {
        if ( !match( cat::keyword, "while" ) )
            return {};

        location loc = peek().loc;
        fetch();
        require( cat::punct, "(" );

        auto e = parse_expr();
        if ( !e )
            error( "Empty condition in while" );

        require( cat::punct, ")" );
        auto body = parse_stmt();
        if ( !body )
            error( "Empty body in while" );

        return ast::stmt{
            .loc = loc,
            .data = ast::while_stmt{
                .cond = std::move( e.value() ),
                .body = make_stmt_ptr( std::move( body.value() ) ),
            }
        };
    }

    std::optional< ast::stmt > parser::parse_do_while()
    {
        if ( !match( cat::keyword, "do" ) )
            return {};

        location loc = peek().loc;
        fetch();
        auto s = parse_stmt();
        if ( !s )
            error( "Empty body in do_while" );

        require( cat::keyword, "while" );
        require( cat::punct, "(" );
        auto e = parse_expr();
        if ( !e )
            error( "Empty condition in do_while" );

        require( cat::punct, ")" );
        require( cat::punct, ";" );

        return ast::stmt{
            .loc = loc,
            .data = ast::do_while_stmt{
                .cond = std::move( e.value() ),
                .body = make_stmt_ptr( std::move( s.value() ) ),
            }
        };
    }

    std::optional< ast::stmt > parser::parse_loop_stmt()
    {
        std::optional< ast::stmt > res{};
        if ( ( res = parse_for() )
            || ( res = parse_while() )
            || ( res = parse_do_while() ) )
            return res;

        return {};
    }

    std::vector< ast::param > parser::parse_param_list()
    {
        std::vector< ast::param > params{};

        while ( !match( cat::punct, ")" ) )
        {
            if ( !params.empty() )
                require( cat::punct, "," );

            auto id = require( cat::ident );
            params.push_back( ast::param{ .name = id.data } );
        }

        return params;
    }

    std::optional< ast::var_declarator > parser::parse_var_declarator()
    {
        if ( !match( cat::ident ) )
            return {};

        auto id = fetch();
        ast::var_declarator decl{ .name = id.data, .init = std::nullopt };

        if ( !match( cat::punct, "=" ) )
            return decl;

        fetch();
        auto e = parse_expr();
        if ( !e )
            error( "Expected initializer expression after '='" );

        decl.init = std::move( e.value() );
        return decl;
    }

    std::optional< ast::stmt > parser::parse_var_decl()
    {
        if ( !match_any( cat::keyword, "let", "var", "const" ) )
            return {};

        auto t = fetch();
        auto kind = ast::var_declaration::kind_t::var;
        if ( t.data == "let" )   kind = ast::var_declaration::kind_t::let;
        if ( t.data == "const" ) kind = ast::var_declaration::kind_t::constant;
        if ( t.data == "var" ) error( "Var not supported yet" );

        ast::var_declaration decl{ .kind = kind, .declarators = {} };

        while ( true )
        {
            auto d = parse_var_declarator();
            if ( !d )
                error( "Expected variable name in declaration" );

            decl.declarators.push_back( std::move( d.value() ) );

            if ( !match( cat::punct, "," ) )
                break;
            fetch();
        }

        require( cat::punct, ";" );
        return ast::stmt{ .data = std::move( decl ) };
    }

    std::optional< ast::stmt > parser::parse_stmt()
    {
        std::optional< ast::stmt > res{};
        if ( ( res = parse_loop_stmt() )
            || ( res = parse_block() )
            || ( res = parse_fn_decl_stmt() )
            || ( res = parse_if() )
            || ( res = parse_ret() )
            || ( res = parse_control_stmt() )
            || ( res = parse_var_decl() )
            || ( res = parse_expr_stmt() ) )
            return res;

        return {};
    }
}