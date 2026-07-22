#pragma once

#include <format>
#include <iostream>
#include <vector>
#include <optional>
#include <memory>
#include <sstream>

#include "ast.hpp"
#include "lexer.hpp"

namespace jsc
{

struct parser : token_sink
{
    using cat = token::cat_t;
    using op_kind = jsc::op_kind;
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

    op_kind op_kind_from_str( std::string_view data )
    {
        if ( data == "+" ) return jsc::ADD;
        if ( data == "-" ) return jsc::SUB;
        if ( data == "*" ) return jsc::MUL;
        if ( data == "/" ) return jsc::DIV;
        if ( data == "%" ) return jsc::MOD;
        if ( data == "<<" ) return jsc::SHL;
        if ( data == ">>" ) return jsc::SHR;
        if ( data == "==" ) return jsc::EQ;
        if ( data == "===" ) return jsc::EQ;
        if ( data == "!=" ) return jsc::NEQ;
        if ( data == "!==" ) return jsc::NEQ;
        if ( data == "<" ) return jsc::LT;
        if ( data == "<=" ) return jsc::LEQ;
        if ( data == ">" ) return jsc::GT;
        if ( data == ">=" ) return jsc::GEQ;
        if ( data == "!" ) return jsc::NOT;
        if ( data == "&&" ) return jsc::AND;
        if ( data == "||" ) return jsc::OR;
        error( "Unknown operator:", data );
        return jsc::ADD;
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
