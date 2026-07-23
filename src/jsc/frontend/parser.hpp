#pragma once

#include <format>
#include <iostream>
#include <vector>
#include <optional>
#include <memory>
#include <sstream>

#include "ast.hpp"
#include "lexer.hpp"

// TODO: maybe think whether we should take V8's frontend
namespace qthu::jsc
{

struct parser : token_sink
{
    using cat = token::cat_t;
    using op_kind = jsc::op_kind;
    using expr = ast::expr;
    using stmt = ast::stmt;
    using fn_declaration = ast::fn_declaration;
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

            std::optional< ast::stmt > s = parse_stmt();
            if ( !s )
                error( "Parse error " );

            prog.statements.push_back( std::make_unique< stmt >( std::move( s.value() ) ) );
        }

        return prog;
    }

    ast::expr_ptr make_expr_ptr( ast::expr e )
    {
        return std::make_unique< ast::expr >( std::move( e ) );
    }

    ast::stmt_ptr make_stmt_ptr( ast::stmt s )
    {
        return std::make_unique< ast::stmt >( std::move( s ) );
    }

    ast::expr make_binary( ast::expr lhs, jsc::op_kind op, ast::expr rhs )
    {
        location loc = lhs.loc;
        return ast::expr{
            .loc = loc,
            .data = ast::binary{
                .op = op,
                .left = make_expr_ptr( std::move( lhs ) ),
                .right = make_expr_ptr( std::move( rhs ) ),
            }
        };
    }

    expr make_increment_expr( expr target, bool is_incr );

    expr make_compound_assign( expr lhs, std::string_view compound_op, expr rhs );

    std::vector< ast::param > parse_param_list();

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

    std::optional< stmt > parse_fn_decl_stmt();

    std::optional< stmt > parse_block();

    std::optional< stmt > parse_if();

    std::optional< stmt > parse_expr_stmt();

    std::optional< stmt > parse_ret();

    std::optional< stmt > parse_control_stmt();

    std::optional< stmt > parse_for();

    std::optional< stmt > parse_while();

    std::optional< stmt > parse_do_while();

    std::optional< stmt > parse_loop_stmt();

    std::optional< ast::var_declarator > parse_var_declarator();
    
    std::optional< stmt > parse_var_decl();

    std::optional< stmt > parse_stmt();

    std::optional< fn_declaration > parse_fn_decl();
};

}
