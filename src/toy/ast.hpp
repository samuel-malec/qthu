#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace toy 
{

struct expr
{
    enum class kind_t
    {
        identifier,
        immediate,
        call,
        unary,
        binary,
        assignment,
    } kind = kind_t::identifier;

    std::string_view lit{};
    int32_t value = 0;
    std::string op{};
    std::vector< expr > subs{};

    static expr make_identifier( std::string_view name )
    {
        expr e;
        e.kind = kind_t::identifier;
        e.lit = name;
        return e;
    }

    static expr make_immediate( std::string_view lit, int32_t value )
    {
        expr e;
        e.kind = kind_t::immediate;
        e.lit = lit;
        e.value = value;
        return e;
    }

    static expr make_call( std::string_view callee, std::vector< expr > args )
    {
        expr e;
        e.kind = kind_t::call;
        e.lit = callee;
        e.subs = std::move( args );
        return e;
    }

    static expr make_unary( std::string op, expr sub )
    {
        expr e;
        e.kind = kind_t::unary;
        e.op = std::move( op );
        e.subs.push_back( std::move( sub ) );
        return e;
    }

    static expr make_binary( std::string op, expr lhs, expr rhs )
    {
        expr e;
        e.kind = kind_t::binary;
        e.op = std::move( op );
        e.subs.push_back( std::move( lhs ) );
        e.subs.push_back( std::move( rhs ) );
        return e;
    }

    static expr make_assignment( expr lhs, expr rhs )
    {
        expr e;
        e.kind = kind_t::assignment;
        e.op = "=";
        e.subs.push_back( std::move( lhs ) );
        e.subs.push_back( std::move( rhs ) );
        return e;
    }
};

struct stmt
{
    enum class kind_t
    {
        expression,
        ret,
        if_stmt,
    } kind = kind_t::expression;

    expr value{};
    expr condition{};
    std::vector< stmt > then_body{};
    std::vector< stmt > else_body{};
};

struct function_decl
{
    std::string_view name{};
    std::vector< std::string_view > params{};
    std::vector< stmt > body{};
};

struct program
{
    std::vector< function_decl > functions{};
};

inline std::string_view expr_kind_name( expr::kind_t k )
{
    using kind = expr::kind_t;
    switch ( k )
    {
    case kind::identifier:
        return "identifier";
    case kind::immediate:
        return "immediate";
    case kind::call:
        return "call";
    case kind::unary:
        return "unary";
    case kind::binary:
        return "binary";
    case kind::assignment:
        return "assignment";
    }
    return "unknown";
}

inline std::string print_expr( const expr& e )
{
    std::string out;
    out += std::string( expr_kind_name( e.kind ) );

    if ( e.kind == expr::kind_t::identifier || e.kind == expr::kind_t::call )
        out += "(" + std::string( e.lit ) + ")";
    else if ( e.kind == expr::kind_t::immediate )
        out += "(" + std::to_string( e.value ) + ")";
    else if ( !e.op.empty() )
        out += "(" + e.op + ")";

    if ( !e.subs.empty() )
    {
        out += "[";
        for ( std::size_t i = 0; i < e.subs.size(); ++i )
        {
            if ( i )
                out += ", ";
            out += print_expr( e.subs[ i ] );
        }
        out += "]";
    }

    return out;
}

inline std::string print( const program& p )
{
    auto indent = []( int level ) -> std::string
    { return std::string( static_cast< std::size_t >( level ) * 2, ' ' ); };

    std::function< void( const stmt&, int, std::string& ) > print_stmt;
    print_stmt = [ & ]( const stmt& s, int level, std::string& out )
    {
        out += indent( level );
        if ( s.kind == stmt::kind_t::ret )
        {
            out += "return ";
            out += print_expr( s.value );
            out += "\n";
            return;
        }

        if ( s.kind == stmt::kind_t::expression )
        {
            out += print_expr( s.value );
            out += "\n";
            return;
        }

        out += "if ";
        out += print_expr( s.condition );
        out += "\n";
        out += indent( level );
        out += "{\n";
        for ( const auto& nested : s.then_body )
            print_stmt( nested, level + 1, out );
        out += indent( level );
        out += "}\n";

        if ( !s.else_body.empty() )
        {
            out += indent( level );
            out += "else\n";
            out += indent( level );
            out += "{\n";
            for ( const auto& nested : s.else_body )
                print_stmt( nested, level + 1, out );
            out += indent( level );
            out += "}\n";
        }
    };

    std::string out;
    for ( const auto& fn : p.functions )
    {
        out += "function " + std::string( fn.name ) + "(";
        for ( std::size_t i = 0; i < fn.params.size(); ++i )
        {
            if ( i )
                out += ", ";
            out += std::string( fn.params[ i ] );
        }
        out += ")\n";

        for ( const auto& s : fn.body )
            print_stmt( s, 1, out );
    }
    return out;
}

} // namespace jsc
