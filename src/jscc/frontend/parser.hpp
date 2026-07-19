#pragma once

#include <format>
#include <iostream>
#include <vector>
#include <optional>
#include <memory>
#include <sstream>

#include "ast.hpp"
#include "lexer.hpp"

namespace jscc
{

struct parser : token_sink
{
    using cat = token::cat_t;
    using op_kind = ast::op_kind;
    using expr = ast::expr;
    using stmt = ast::stmt;
    using toplevel = ast::toplevel;
    using fn_decl = ast::fn_decl;
    using var_decl = ast::var_decl;
    using program = ast::program;

    token current;
    size_t pos = 0;
    source_ptr doc;
    lexer lex;

    parser( source_ptr doc ) : doc{ doc }, lex{ doc, *this } {}

    template < typename... Args >
    void error( Args... args )
    {
        std::stringstream buf{};
        ( ( ( buf << " " ) << args ), ... );

        if ( current.cat != cat::invalid )
            buf << current.loc << '\n';
        throw std::runtime_error( buf.str() );
    }

    token require( cat c, std::string_view data = "" )
    {
        auto tok = fetch();
        if ( tok.cat != c )
            error( tok, "Expected category: ", c, data.empty() ? "" : ", data: ", data );

        if ( !data.empty() && tok.data != data )
            error( tok, "Expected: ", data );

        return tok;
    }

    bool match_decl_keyword()
    {
        return match( cat::keyword, "let" )
            || match( cat::keyword, "const" )
            || match( cat::keyword, "var" );
    }

    op_kind op_kind_from_str( std::string_view data )
    {
        if ( data == "+" ) return ast::ADD;
        if ( data == "-" ) return ast::SUB;
        if ( data == "*" ) return ast::MUL;
        if ( data == "/" ) return ast::DIV;
        if ( data == "%" ) return ast::MOD;
        if ( data == "<<" ) return ast::SHL;
        if ( data == ">>" ) return ast::SHR;
        if ( data == "==" ) return ast::EQ;
        if ( data == "===" ) return ast::EQ;   
        if ( data == "!=" ) return ast::NEQ;
        if ( data == "!==" ) return ast::NEQ;  
        if ( data == "<" ) return ast::LT;
        if ( data == "<=" ) return ast::LEQ;
        if ( data == ">" ) return ast::GT;
        if ( data == ">=" ) return ast::GEQ;
        if ( data == "!" ) return ast::NOT;
        if ( data == "&&" ) return ast::AND;
        if ( data == "||" ) return ast::OR;
        error( "Unknown operator:", data );
        return ast::ADD;
    }

    bool match( cat c, std::string_view data = "" )
    {
        auto tok = peek();
        if ( tok.cat != c )
            return false;

        if ( data.empty() )
            return true;

        return tok.data == data;
    }

    template< typename... Args >
    std::optional< token > match_any( cat c, Args... args )
    {
        auto tok = peek();
        if ( tok.cat != c )
            return {};

        if ( ( ( tok.data == args ) || ... ) )
            return tok;

        return {};
    }

    token peek()
    {
        if ( current.cat != cat::invalid )
            return current;

        assert( !lex.empty() );
        while ( !lex.empty() && current.cat == cat::invalid )
            lex.next();

        return current;
    }

    token fetch()
    {
        auto rv = peek();
        current = {};
        return rv;
    }

    void push( token t ) override
    {
        current = std::move( t );
    }

    ast::program parse()
    {
        program prog{};
        while ( !lex.empty() )
        {
            if ( peek().cat == cat::invalid )
                break;

            std::optional< toplevel > d = parse_toplevel();
            if ( !d )
                error( "Expected a toplevel declaration " );

            prog.toplevel_items.push_back( d.value() );
        }

        return prog;
    }

    expr make_expr_node( expr::cat_t cat );

    expr make_increment_expr( expr target, bool is_incr );

    expr make_compound_assign( expr lhs, std::string_view compound_op, expr rhs );

    std::optional< expr > parse_primary();

    std::optional< expr > parse_postfix();

    std::optional< expr > parse_unary();

    std::optional< expr > parse_factor();

    std::optional< expr > parse_term();

    std::optional< expr > parse_shift();

    std::optional< expr > parse_comparison();

    std::optional< expr > parse_equality();

    std::optional< expr > parse_assignment();

    std::optional< expr > parse_and();

    std::optional< expr > parse_or();

    std::optional< expr > parse_expr();

    std::optional< stmt > parse_block();

    std::optional< stmt > parse_if();

    std::optional< stmt > parse_expr_stmt();

    std::optional< stmt > parse_ret();

    std::optional< stmt > parse_control_stmt();

    std::optional< stmt > parse_for();

    std::optional< stmt > parse_while();

    std::optional< stmt > parse_do_while();

    std::optional< stmt > parse_loop_stmt();

    std::vector< var_decl > parse_var_decl_list();

    std::optional< var_decl > parse_var_decl_info();

    std::optional< stmt > parse_var_decl();

    std::optional< stmt > parse_stmt();

    std::optional< fn_decl > parse_fn_decl();

    std::optional< toplevel > parse_toplevel();
};

}
