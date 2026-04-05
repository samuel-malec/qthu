#pragma once

#include <format>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "ast.hpp"
#include "token.hpp"

namespace toy
{

struct parser
{
    using cat = token::category;

    const std::vector< token >& tokens;
    std::size_t pos = 0;

    explicit parser( const std::vector< token >& toks ) : tokens( toks ) { }

    bool at_end() const
    {
        return pos >= tokens.size();
    }

    const token* peek( std::size_t offset = 0 ) const
    {
        const auto i = pos + offset;
        if ( i >= tokens.size() )
            return nullptr;
        return &tokens[ i ];
    }

    [[noreturn]] void fail( std::string_view msg ) const
    {
        if ( auto t = peek() )
            throw std::runtime_error( std::format( "parse error at token '{}': {}", std::string( t->lit ), msg ) );
        throw std::runtime_error( std::format( "parse error at end of input: {}", msg ) );
    }

    bool match( std::string_view lit )
    {
        if ( auto t = peek(); t && t->is( lit ) )
            return ++pos, true;
        return false;
    }

    bool match( cat c )
    {
        if ( auto t = peek(); t && t->is( c ) )
            return ++pos, true;
        return false;
    }

    const token& expect( std::string_view lit, std::string_view what = "token" )
    {
        auto t = peek();
        if ( !t || !t->is( lit ) )
            fail( std::format( "expected {} '{}'", what, std::string( lit ) ) );
        ++pos;
        return tokens[ pos - 1 ];
    }

    const token& expect( cat c, std::string_view what )
    {
        auto t = peek();
        if ( !t || !t->is( c ) )
            fail( std::format( "expected {}", what ) );
        ++pos;
        return tokens[ pos - 1 ];
    }

    program parse_program()
    {
        program p;
        while ( !at_end() )
            p.functions.push_back( parse_function() );
        return p;
    }

    function_decl parse_function()
    {
        expect( "function", "keyword" );
        auto name = expect( cat::ident, "function name" ).lit;
        expect( "(", "'('" );

        std::vector< std::string_view > params;
        if ( !match( ")" ) )
        {
            params.push_back( expect( cat::ident, "parameter" ).lit );
            while ( match( "," ) )
                params.push_back( expect( cat::ident, "parameter" ).lit );
            expect( ")", "')'" );
        }
        
        auto body = parse_block();
        return function_decl{.name = name, .params = std::move( params ), .body = std::move( body ) };
    }

    std::vector< stmt > parse_block()
    {
        expect( "{", "'{'" );
        std::vector< stmt > out;
        while ( !match( "}" ) )
        {
            if ( at_end() )
                fail( "expected '}' to close block" );
            out.push_back( parse_statement() );
        }
        return out;
    }

    stmt parse_statement()
    {
        if ( match( "if" ) )
            return parse_if_statement();

        if ( match( "return" ) )
        {
            auto e = parse_expression();
            expect( ";", "';'" );
            return stmt{ .kind = stmt::kind_t::ret, .value = std::move( e ) };
        }

        auto e = parse_expression();
        expect( ";", "';'" );
        return stmt{ .kind = stmt::kind_t::expression, .value = std::move( e ) };
    }

    std::vector< stmt > parse_statement_or_block()
    {
        if ( auto t = peek(); t && t->is( "{" ) )
            return parse_block();
        return { parse_statement() };
    }

    stmt parse_if_statement()
    {
        expect( "(", "'('" );
        auto cond = parse_expression();
        expect( ")", "')'" );

        auto then_body = parse_statement_or_block();
        std::vector< stmt > else_body;
        if ( match( "else" ) )
            else_body = parse_statement_or_block();

        stmt s;
        s.kind = stmt::kind_t::if_stmt;
        s.condition = std::move( cond );
        s.then_body = std::move( then_body );
        s.else_body = std::move( else_body );
        return s;
    }

    expr parse_expression()
    {
        return parse_assignment();
    }

    expr parse_assignment()
    {
        auto lhs = parse_logical_or();
        if ( match( "=" ) )
        {
            auto rhs = parse_assignment();
            return expr::make_assignment( std::move( lhs ), std::move( rhs ) );
        }
        return lhs;
    }

    expr parse_logical_or()
    {
        auto e = parse_logical_and();
        while ( match( "||" ) )
            e = expr::make_binary( "||", std::move( e ), parse_logical_and() );
        return e;
    }

    expr parse_logical_and()
    {
        auto e = parse_equality();
        while ( match( "&&" ) )
            e = expr::make_binary( "&&", std::move( e ), parse_equality() );
        return e;
    }

    expr parse_equality()
    {
        auto e = parse_relational();
        while ( true )
        {
            if ( match( "==" ) )
                e = expr::make_binary( "==", std::move( e ), parse_relational() );
            else if ( match( "!=" ) )
                e = expr::make_binary( "!=", std::move( e ), parse_relational() );
            else
                break;
        }
        return e;
    }

    expr parse_relational()
    {
        auto e = parse_additive();
        while ( true )
        {
            if ( match( "<" ) )
                e = expr::make_binary( "<", std::move( e ), parse_additive() );
            else if ( match( "<=" ) )
                e = expr::make_binary( "<=", std::move( e ), parse_additive() );
            else if ( match( ">" ) )
                e = expr::make_binary( ">", std::move( e ), parse_additive() );
            else if ( match( ">=" ) )
                e = expr::make_binary( ">=", std::move( e ), parse_additive() );
            else
                break;
        }
        return e;
    }

    expr parse_additive()
    {
        auto e = parse_multiplicative();
        while ( true )
        {
            if ( match( "+" ) )
                e = expr::make_binary( "+", std::move( e ), parse_multiplicative() );
            else if ( match( "-" ) )
                e = expr::make_binary( "-", std::move( e ), parse_multiplicative() );
            else
                break;
        }
        return e;
    }

    expr parse_multiplicative()
    {
        auto e = parse_unary();
        while ( true )
        {
            if ( match( "*" ) )
                e = expr::make_binary( "*", std::move( e ), parse_unary() );
            else if ( match( "/" ) )
                e = expr::make_binary( "/", std::move( e ), parse_unary() );
            else if ( match( "%" ) )
                e = expr::make_binary( "%", std::move( e ), parse_unary() );
            else
                break;
        }
        return e;
    }

    expr parse_unary()
    {
        if ( match( "+" ) )
            return expr::make_unary( "+", parse_unary() );
        if ( match( "-" ) )
            return expr::make_unary( "-", parse_unary() );
        return parse_postfix();
    }

    expr parse_postfix()
    {
        auto e = parse_primary();

        while ( match( "(" ) )
        {
            std::vector< expr > args;
            if ( !match( ")" ) )
            {
                args.push_back( parse_expression() );
                while ( match( "," ) )
                    args.push_back( parse_expression() );
                expect( ")", "')'" );
            }

            if ( e.kind != expr::kind_t::identifier )
                fail( "function call target must be an identifier" );

            e = expr::make_call( e.lit, std::move( args ) );
        }

        return e;
    }

    expr parse_primary()
    {
        if ( auto t = peek(); t && t->is( cat::number ) )
        {
            ++pos;
            return expr::make_immediate( t->lit, t->value );
        }

        if ( auto t = peek(); t && t->is( cat::ident ) )
        {
            ++pos;
            return expr::make_identifier( t->lit );
        }

        if ( match( "(" ) )
        {
            auto e = parse_expression();
            expect( ")", "')'" );
            return e;
        }

        fail( "expected primary expression" );
    }
};

inline program parse( const std::vector< token >& tokens )
{
    parser p( tokens );
    return p.parse_program();
}

} // namespace jsc
