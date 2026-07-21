#include <iostream>
#include <charconv>

#include "parser.hpp"

namespace jscc
{
    using cat = token::cat_t;
    using expr = ast::expr;
    using stmt = ast::stmt;
    using toplevel = ast::toplevel;
    using var_decl = ast::var_decl;
    using fn_decl = ast::fn_decl;
    using program = ast::program;

    expr parser::make_expr_node( expr::cat_t cat )
    {
        return expr{
            .cat = cat,
            .val_kind = ast::expr::val_kind_t::lvalue,
            .src_loc = peek().loc,
            .val = std::monostate{},
            .id = {},
            .subs = {},
            .op = jscc::ADD,
        };
    }

    expr parser::make_increment_expr( expr target, bool is_incr )
    {
        expr one = make_expr_node( expr::num_lit );
        one.val = uint64_t{ 1 };

        expr bin = make_expr_node( expr::binary );
        bin.op = is_incr ? ADD : SUB;
        bin.subs.push_back( target );
        bin.subs.push_back( one );

        expr assign = make_expr_node( expr::assign );
        assign.op = EQ;
        assign.subs.push_back( target );
        assign.subs.push_back( bin );
        return assign;
    }

    expr parser::make_compound_assign( expr lhs, std::string_view compound_op, expr rhs )
    {
        std::string_view base_op = compound_op.substr( 0, compound_op.size() - 1 );

        expr bin = make_expr_node( expr::binary );
        bin.op = op_kind_from_str( base_op );
        bin.subs.push_back( lhs );
        bin.subs.push_back( rhs );

        expr assign = make_expr_node( expr::assign );
        assign.op = EQ;
        assign.subs.push_back( lhs );
        assign.subs.push_back( bin );
        return assign;
    }

    std::optional< expr > parser::parse_primary()
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

            auto e = make_expr_node( expr::num_lit );
            e.val = n;
            e.val_kind = ast::expr::val_kind_t::rvalue;
            return e;
        }

        if ( auto t = match_any( cat::keyword, "true", "false" ) )
        {
            fetch();
            bool value = t.value().data == "true";
            auto e = make_expr_node( expr::bool_lit );
            e.val = value;
            e.val_kind = ast::expr::val_kind_t::rvalue;
            return e;
        }

        if ( match( cat::ident ) )
        {
            auto tok = fetch();
            auto e = make_expr_node( expr::identifier );
            e.id = tok.data;
            return e;
        }

        return {};
    }

    std::optional< expr > parser::parse_postfix()
    {
        auto e = parse_primary();
        if ( !e )
            return {};

        while ( true )
        {
            if ( match( cat::punct, "(" ) )
            {
                fetch();

                expr call = make_expr_node( expr::call );
                call.id = e->id;
                call.subs.push_back( e.value() );

                if ( !match( cat::punct, ")" ) )
                {
                    while ( true )
                    {
                        auto arg = parse_expr();
                        if ( !arg )
                            error( "Expected function argument" );

                        call.subs.push_back( arg.value() );

                        if ( !match( cat::punct, "," ) )
                            break;
                        fetch();
                    }
                }

                require( cat::punct, ")" );
                e = call;
                continue;
            }

            if ( auto t = match_any( cat::punct, "++", "--" ) )
            {
                fetch();
                e = make_increment_expr( e.value(), t->data == "++" );
                continue;
            }

            break;
        }

        return e;
    }

    std::optional< expr > parser::parse_unary()
    {
        if ( auto t = match_any( cat::punct, "!", "-", "+" ) )
        {
            fetch();
            auto rhs = parse_unary();
            if ( !rhs )
                error( "Expected unary operand" );

            expr out = make_expr_node( expr::unary );
            out.op = op_kind_from_str( t->data );
            out.subs.push_back( rhs.value() );
            return out;
        }

        if ( auto t = match_any( cat::punct, "++", "--" ) )
        {
            fetch();
            auto rhs = parse_unary();
            if ( !rhs )
                error( "Expected operand after ++/--" );

            return make_increment_expr( rhs.value(), t->data == "++" );
        }

        return parse_postfix();
    }

    std::optional< expr > parser::parse_factor()
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

            expr tmp = make_expr_node( expr::binary );
            tmp.op = op_kind_from_str( t->data );
            tmp.subs.push_back( e.value() );
            tmp.subs.push_back( rhs.value() );
            e = std::move( tmp );
        }

        return e;
    }

    std::optional< expr > parser::parse_term()
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

            expr tmp = make_expr_node( expr::binary );
            tmp.op = op_kind_from_str( t->data );
            tmp.subs.push_back( e.value() );
            tmp.subs.push_back( rhs.value() );
            e = std::move( tmp );
        }

        return e;
    }

    std::optional< expr > parser::parse_shift()
    {
        auto e = parse_term();
        if ( !e )
            return {};

        while( auto t = match_any( cat::punct, ">>", "<<" ) )
        {
            fetch();
            auto rhs = parse_term();
            if ( !rhs )
                error( "Expected rhs for shift expression" );

            expr tmp = make_expr_node( expr::binary );
            tmp.op = op_kind_from_str( t->data );
            tmp.subs.push_back( e.value() );
            tmp.subs.push_back( rhs.value() );
            e = std::move( tmp );
        }

        return e;
    }

    std::optional< expr > parser::parse_comparison()
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

            auto tmp = make_expr_node( expr::relational );
            tmp.op = op_kind_from_str( t->data );
            tmp.subs.push_back( e.value() );
            tmp.subs.push_back( rhs.value() );
            e = std::move( tmp );
        }

        return e;
    }

    std::optional< expr > parser::parse_equality()
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

            auto tmp = make_expr_node( expr::relational );
            tmp.op = op_kind_from_str( t.value().data );
            tmp.subs.push_back( e.value() );
            tmp.subs.push_back( rhs.value() );
            e = tmp;
        }

        return e;
    }

    std::optional< expr > parser::parse_assignment()
    {
        auto e = parse_equality();
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
                auto tmp = make_expr_node( expr::assign );
                tmp.op = ::jscc::EQ;
                tmp.subs.push_back( e.value() );
                tmp.subs.push_back( rhs.value() );
                e = std::move( tmp );
            }
            else
                e = make_compound_assign( e.value(), t->data, rhs.value() );
        }

        return e;
    }

    std::optional< expr > parser::parse_and()
    {
        auto e = parse_assignment();
        if ( !e )
            return {};

        while ( auto t = match_any( cat::punct, "&&" ) )
        {
            fetch();
            auto rhs = parse_assignment();
            if ( !rhs )
                error( "Expected rhs for assignment expression" );

            auto tmp = make_expr_node( expr::binary );
            tmp.op = op_kind_from_str( t->data );
            tmp.subs.push_back( e.value() );
            tmp.subs.push_back( rhs.value() );
            e = std::move( tmp );
        }

        return e;
    }

    std::optional< expr > parser::parse_or()
    {
        auto e = parse_and();
        if ( !e )
            return {};

        while ( auto t = match_any( cat::punct, "||" ) )
        {
            fetch();
            auto rhs = parse_assignment();
            if ( !rhs )
                error( "Expected rhs for assignment expression" );

            auto tmp = make_expr_node( expr::binary );
            tmp.op = op_kind_from_str( t->data );
            tmp.subs.push_back( e.value() );
            tmp.subs.push_back( rhs.value() );
            e = std::move( tmp );
        }

        return e;
    }

    std::optional< expr > parser::parse_expr()
    {
        return parse_or();
    }

    std::optional< stmt > parser::parse_expr_stmt()
    {
        auto e = parse_expr();
        if ( !e )
            return {};

        require( cat::punct, ";" );
        return stmt{ .cat = stmt::expr_stmt, .e = e.value() };
    }

    std::optional< stmt > parser::parse_block()
    {
        if ( !match( cat::punct, "{" ) )
            return {};

        fetch();
        stmt res{ .cat = stmt::block };

        while ( !match( cat::punct, "}" ) )
        {
            auto s = parse_stmt();
            if ( !s )
                error( "Parsing statement inside a block: " );

            res.subs.push_back( s.value() );
        }

        fetch();
        return res;
    }

    std::optional< stmt > parser::parse_if()
    {
        if ( !match( cat::keyword, "if" ) )
            return {};

        fetch();
        require( cat::punct, "(" );

        auto cond = parse_expr();
        if ( !cond )
            error( "Expected condition after if" );

        require( cat::punct, ")" );
        auto then_body = parse_stmt();
        if ( !then_body )
            error( "Empty body inside if block" );

        stmt res{ .cat = stmt::if_stmt, .e = cond.value() };
        res.subs.push_back( std::move( then_body.value() ) );
        if ( !match( cat::keyword, "else" ) )
            return res;

        fetch();
        auto else_body = parse_stmt();
        if ( !else_body )
            error( "Empty else body" );

        res.subs.push_back( std::move( else_body.value() ) );
        return res;
    }

    std::optional< stmt > parser::parse_ret()
    {
        if ( !match( cat::keyword, "return" ) )
            return {};

        fetch();
        auto res = stmt{ .cat = stmt::ret };

        if ( match( cat::punct, ";" ) )
        {
            fetch();
            return res;
        }

        auto e = parse_expr();
        if ( !e )
            error( "Expected expression" );

        res.e = e.value();
        require( cat::punct, ";" );
        return res;
    }

    std::optional< stmt > parser::parse_control_stmt()
    {
        if ( match( cat::keyword, "break" ) )
        {
            fetch();
            require( cat::punct, ";" );
            return stmt{ .cat = stmt::brk };
        }

        if ( match( cat::keyword, "continue" ) )
        {
            fetch();
            require( cat::punct, ";" );
            return stmt{ .cat = stmt::cont };
        }

        return {};
    }

    std::optional< stmt > parser::parse_for()
    {
        if ( !match( cat::keyword, "for" ) )
            return {};

        fetch();
        require( cat::punct, "(" );
        stmt init{};

        if ( !match( cat::punct, ";" ) )
        {
            if ( auto v = parse_var_decl() )
                init = std::move( v.value() );
            else
            {
                auto ie = parse_expr();
                if ( !ie )
                    error( "Invalid for loop initializer" );

                init = stmt{ .cat = stmt::expr_stmt, .e = ie.value() };
                require( cat::punct, ";" );
            }
        }
        else
            fetch();

        auto cond_expr = make_expr_node( expr::bool_lit );
        cond_expr.val = true;
        stmt cond{ .cat = stmt::expr_stmt, .e = cond_expr };
        if ( !match( cat::punct, ";" ) )
        {
            auto ce = parse_expr();
            if ( !ce )
                error( "Invalid for loop condition" );

            cond.e = ce.value();
        }

        require( cat::punct, ";" );
        stmt update{ .cat = stmt::expr_stmt };
        if ( !match( cat::punct, ")" ) )
        {
            auto ue = parse_expr();
            if ( !ue )
                error( "Invalid for loop update" );
            update.e = ue.value();
        }

        require( cat::punct, ")" );
        auto body = parse_stmt();
        if ( !body )
            error( "Empty body in for loop" );

        stmt res{ .cat = stmt::for_stmt };
        res.subs.push_back( std::move( init ) );
        res.subs.push_back( std::move( cond ) );
        res.subs.push_back( std::move( update ) );
        res.subs.push_back( std::move( body.value() ) );
        return res;
    }

    std::optional< stmt > parser::parse_while()
    {
        if ( !match( cat::keyword, "while" ) )
            return {};
        fetch();

        require( cat::punct, "(" );
        auto e = parse_expr();
        if ( !e )
            error( "Empty condition in while" );

        require( cat::punct, ")" );
        auto body = parse_stmt();
        if ( !body )
            error( "Empty body in while" );

        stmt res{ .cat = stmt::while_stmt, .e = e.value() };
        res.subs.push_back( std::move( body.value() ) );
        return res;
    }

    std::optional < stmt > parser::parse_do_while()
    {
        if ( !match( cat::keyword, "do" ) )
            return {};

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

        stmt res{ .cat = stmt::do_while_stmt, .e = e.value() };
        res.subs.push_back( std::move( s.value() ) );
        return res;
    }

    std::optional< stmt > parser::parse_loop_stmt()
    {
        std::optional< stmt > res{};
        if ( ( res = parse_for() )
            || ( res = parse_while() )
            || ( res = parse_do_while() ) )
            return res;

        return {};
    }

    std::vector< var_decl > parser::parse_var_decl_list()
    {
        std::vector< var_decl > var_decls{};

        while ( !match( cat::punct, ")" ) )
        {
            if ( !var_decls.empty() )
                require( cat::punct, "," );

            auto id = require( cat::ident );
            var_decls.push_back( var_decl{ .modifier = var_decl::mod_t::let, .name = id.data } );
        }

        return var_decls;
    }
    
    std::optional< var_decl > parser::parse_var_decl_info()
    {
        if ( !match_any( cat::keyword, "let", "var", "const" ) )
            return {};
        
        auto t = fetch();
        auto modifier = var_decl::mod_t::var;
        if ( t.data == "let" )
            modifier = var_decl::mod_t::let;
        else if ( t.data == "const" )
            modifier = var_decl::mod_t::con;

        auto id = require( cat::ident );
        var_decl vdecl{ .modifier = modifier, .name = id.data, .e = std::nullopt };

        if ( match( cat::punct, ";" ) )
            return vdecl;

        if ( !match( cat::punct, "=" ) )
            error( "Unexpected symbol in variable declaration" );

        fetch();
        auto e = parse_expr();
        if ( !e )
            return {};

        vdecl.e = e.value();
        return vdecl;
    }

    std::optional< stmt > parser::parse_var_decl()
    {
        auto vdecl = parse_var_decl_info();
        if ( !vdecl )
            return {};

        require( cat::punct, ";" );
        return stmt{ .cat = stmt::var_dclr, .vdecl = vdecl.value() };
    }

    std::optional< stmt > parser::parse_stmt()
    {
        std::optional< stmt > res{};
        if ( ( res = parse_loop_stmt() )
            || ( res = parse_block() )
            || ( res = parse_if() )
            || ( res = parse_ret() )
            || ( res = parse_control_stmt() )
            || ( res = parse_var_decl() )
            || ( res = parse_expr_stmt() ) )
            return res;

        return {};
    }

    std::optional< fn_decl > parser::parse_fn_decl()
    {
        if ( !match( cat::keyword, "function" ) )
            return {};

        fetch();
        auto id = require( cat::ident );
        require( cat::punct, "(" );

        fn_decl res{ .name = id.data };
        res.params = parse_var_decl_list();

        require( cat::punct, ")" );
        require( cat::punct, "{" );

        while ( !match( cat::punct, "}" ) )
        {
            auto s = parse_stmt();
            if ( !s )
                error( "Parsing statement in function: ", id );
            res.body.push_back( s.value() );
        }

        fetch();
        return res;
    }

    std::optional< toplevel > parser::parse_toplevel()
    {
        std::optional< toplevel > res;
        if ( ( res = parse_fn_decl() ) 
            || ( res = parse_stmt() ) )
            return res;

        return {};
    }
}
