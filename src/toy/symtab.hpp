#pragma once

#include <map>
#include <string>
#include <cstdint>
#include "ast.hpp"

namespace qthu::toy
{

struct loctab
{
    std::map< std::string, uint16_t > args;
    std::map< std::string, uint16_t > locals;
    uint16_t local_count = 0;

    void add_arg( std::string_view name, uint16_t idx ) { args[ std::string( name ) ] = idx; }
    void add_local( std::string_view name )
    {
        if ( args.contains( std::string( name ) ) )
            return;
        auto [ it, inserted ] = locals.emplace( std::string( name ), local_count );
        if ( inserted )
            ++local_count;
    }
};


struct symtab
{
    std::map< std::string, uint16_t > funcs;
    std::map< std::string, loctab > loctabs;
    uint16_t func_count = 0;

    void add_func( std::string_view name ) { funcs[ std::string( name ) ] = func_count++; }
    void add_loctab( std::string_view name, loctab lt ) { loctabs[ std::string( name ) ] = lt; }
};

inline void populate_expr( loctab& st, const expr& e )
{
    for ( const auto& sub : e.subs )
        populate_expr( st, sub );

    switch ( e.kind )
    {
        case expr::kind_t::identifier:
            st.add_local( e.lit );
            break;
        default:
            break;
    }
}

inline void populate_stmt( loctab& st, const stmt& s )
{
    using kind = stmt::kind_t;
    switch ( s.kind )
    {
        case kind::expression:
        case kind::ret:
            populate_expr( st, s.value );
            break;
        case kind::if_stmt:
            populate_expr( st, s.condition );
            for ( const auto& a : s.then_body )
                populate_stmt( st, a );
            for ( const auto& b : s.else_body )
                populate_stmt( st, b );
            break;
        default:
            break;
    }
}

inline loctab populate( const function_decl& func )
{
    loctab st{};
    for ( uint16_t i = 0; i < func.params.size(); ++i )
        st.add_arg( func.params[ i ], i );
    
    for ( const stmt& s : func.body )
        populate_stmt( st, s );
    
    return st;
}

inline symtab populate( const program& prog )
{
    symtab st{};

    for ( const function_decl& fnd : prog.functions )
        if ( fnd.name == "main" )
            st.add_func( fnd.name );

    for ( const function_decl& fnd : prog.functions )
    {
        if ( fnd.name == "main" )
            continue;
        st.add_func( fnd.name );
    }

    for ( const function_decl& fnd : prog.functions )
        st.add_loctab( fnd.name, populate( fnd ) );

    return st;
}

}
