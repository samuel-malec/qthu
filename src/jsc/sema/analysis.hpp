#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../frontend/ast.hpp"

namespace jsc::sema
{

struct name_id
{
    uint32_t value;
    auto operator<=>( const name_id& ) const = default;
};

struct binding_id
{
    uint32_t value;
    auto operator<=>( const binding_id& ) const = default;
};

struct function_id
{
    uint32_t value;
    auto operator<=>( const function_id& ) const = default;
};

struct scope_id
{
    uint32_t value;
    auto operator<=>( const scope_id& ) const = default;
};

template< typename... Args >
inline void error( const location& where, Args&&... args )
{
    std::ostringstream out;
    out << where << ": ";
    ( out << ... << std::forward< Args >( args ) );
    throw std::runtime_error( out.str() );
}

struct binding
{
    binding_id id;
    name_id name;
    ast::var_decl::mod_t declaration_kind;
    scope_id declared_in;
    location declared_at;
    bool initialized = false;
};

struct function
{
    function_id id;
    name_id name;
    const ast::fn_decl* declaration;
    scope_id scope;
    size_t arity;
};

struct scope
{
    enum class kind { global, function, loop, block };

    scope_id id;
    kind category;
    std::optional< scope_id > parent;
    std::optional< scope_id > enclosing_function;
    std::map< name_id, binding_id > bindings;
};

struct analysis_result
{
    std::vector< std::string > names;
    std::vector< binding > bindings;
    std::vector< function > functions;
    std::vector< scope > scopes;

    std::unordered_map< const ast::expr*, binding_id > identifier_bindings;
    std::unordered_map< const ast::expr*, binding_id > assignment_targets;
    std::unordered_map< const ast::expr*, function_id > direct_calls;
    std::unordered_map< const ast::var_decl*, binding_id > declarations;
    std::unordered_map< const ast::stmt*, scope_id > statement_scopes;
    std::unordered_map< const ast::fn_decl*, function_id > function_declarations;
};

struct analyzer
{
    analysis_result result;
    std::map< std::string, name_id, std::less<> > interned_names;
    std::map< name_id, function_id > global_functions;

    name_id intern( std::string_view name )
    {
        if ( auto it = interned_names.find( name ); it != interned_names.end() )
            return it->second;

        name_id id{ static_cast< uint32_t >( result.names.size() ) };
        result.names.emplace_back( name );
        interned_names.emplace( result.names.back(), id );
        return id;
    }

    scope_id add_scope( scope::kind kind, std::optional< scope_id > parent )
    {
        scope_id id{ static_cast< uint32_t >( result.scopes.size() ) };
        std::optional< scope_id > enclosing_function;
        if ( kind == scope::kind::function )
            enclosing_function = id;
        else if ( parent )
            enclosing_function = result.scopes.at( parent->value ).enclosing_function;

        result.scopes.push_back( scope{ id, kind, parent, enclosing_function } );
        return id;
    }

    scope_id declaration_scope( scope_id current, ast::var_decl::mod_t kind ) const
    {
        if ( kind != ast::var_decl::mod_t::var )
            return current;

        for ( auto scope = current;; )
        {
            const auto& candidate = result.scopes.at( scope.value );
            if ( candidate.category == scope::kind::function || candidate.category == scope::kind::global )
                return scope;
            scope = *candidate.parent;
        }
    }

    binding_id declare( const ast::var_decl& declaration, scope_id current, bool initialized )
    {
        const auto target_scope = declaration_scope( current, declaration.modifier );
        auto& target = result.scopes.at( target_scope.value );
        const auto name = intern( declaration.name );
        if ( target.bindings.contains( name ) )
            error( declaration.e ? declaration.e->src_loc : location{}, "redeclaration of ", declaration.name );

        binding_id id{ static_cast< uint32_t >( result.bindings.size() ) };
        result.bindings.push_back( binding{
            id, name, declaration.modifier, target_scope,
            declaration.e ? declaration.e->src_loc : location{}, initialized,
        } );
        target.bindings.emplace( name, id );
        result.declarations.emplace( &declaration, id );
        return id;
    }

    std::optional< binding_id > lookup( scope_id current, std::string_view name ) const
    {
        const auto it = interned_names.find( name );
        if ( it == interned_names.end() )
            return {};

        for ( auto scope = std::optional< scope_id >{ current }; scope; )
        {
            const auto& current_scope = result.scopes.at( scope->value );
            if ( auto binding = current_scope.bindings.find( it->second ); binding != current_scope.bindings.end() )
                return binding->second;
            scope = current_scope.parent;
        }
        return {};
    }

    bool in_loop( scope_id current ) const
    {
        for ( auto scope = std::optional< scope_id >{ current }; scope; scope = result.scopes.at( scope->value ).parent )
            if ( result.scopes.at( scope->value ).category == scope::kind::loop )
                return true;
        return false;
    }

    bool in_function( scope_id current ) const
    {
        return result.scopes.at( current.value ).enclosing_function.has_value();
    }

    void build_statement( const ast::stmt& statement, scope_id current )
    {
        result.statement_scopes.emplace( &statement, current );
        switch ( statement.cat )
        {
            case ast::stmt::var_dclr:
                declare( statement.vdecl, current, false );
                return;

            case ast::stmt::block:
            {
                const auto child = add_scope( scope::kind::block, current );
                result.statement_scopes[ &statement ] = child;
                for ( const auto& substatement : statement.subs )
                    build_statement( substatement, child );
                return;
            }

            case ast::stmt::for_stmt:
            case ast::stmt::while_stmt:
            case ast::stmt::do_while_stmt:
            {
                const auto child = add_scope( scope::kind::loop, current );
                result.statement_scopes[ &statement ] = child;
                for ( const auto& substatement : statement.subs )
                    build_statement( substatement, child );
                return;
            }

            default:
                for ( const auto& substatement : statement.subs )
                    build_statement( substatement, current );
                return;
        }
    }

    void resolve_expression( const ast::expr& expression, scope_id current )
    {
        switch ( expression.cat )
        {
            case ast::expr::num_lit:
            case ast::expr::bool_lit:
                return;

            case ast::expr::identifier:
            {
                const auto binding = lookup( current, expression.id );
                if ( !binding )
                    error( expression.src_loc, "unknown identifier: ", expression.id );
                if ( !result.bindings.at( binding->value ).initialized )
                    error( expression.src_loc, "use of uninitialized binding: ", expression.id );
                result.identifier_bindings.emplace( &expression, *binding );
                return;
            }

            case ast::expr::assign:
            {
                if ( expression.subs.size() != 2 || expression.subs[ 0 ].cat != ast::expr::identifier )
                    error( expression.src_loc, "assignment target must be an identifier in the current subset" );
                const auto binding = lookup( current, expression.subs[ 0 ].id );
                if ( !binding )
                    error( expression.subs[ 0 ].src_loc, "unknown identifier: ", expression.subs[ 0 ].id );
                if ( result.bindings.at( binding->value ).declaration_kind == ast::var_decl::mod_t::con )
                    error( expression.src_loc, "assignment to const binding: ", expression.subs[ 0 ].id );
                result.assignment_targets.emplace( &expression, *binding );
                resolve_expression( expression.subs[ 1 ], current );
                result.bindings.at( binding->value ).initialized = true;
                return;
            }

            case ast::expr::call:
            {
                if ( expression.subs.empty() || expression.subs[ 0 ].cat != ast::expr::identifier )
                    error( expression.src_loc, "only direct named calls are supported" );
                const auto name = intern( expression.subs[ 0 ].id );
                const auto function = global_functions.find( name );
                if ( function == global_functions.end() )
                    error( expression.src_loc, "unknown function: ", expression.subs[ 0 ].id );
                const auto& signature = result.functions.at( function->second.value );
                if ( expression.subs.size() - 1 != signature.arity )
                    error( expression.src_loc, "wrong number of arguments to ", expression.subs[ 0 ].id );
                result.direct_calls.emplace( &expression, function->second );
                for ( size_t i = 1; i < expression.subs.size(); ++i )
                    resolve_expression( expression.subs[ i ], current );
                return;
            }

            default:
                for ( const auto& operand : expression.subs )
                    resolve_expression( operand, current );
                return;
        }
    }

    void resolve_statement( const ast::stmt& statement, scope_id parent_scope )
    {
        const auto current = result.statement_scopes.at( &statement );
        switch ( statement.cat )
        {
            case ast::stmt::var_dclr:
            {
                const auto binding = result.declarations.at( &statement.vdecl );
                if ( statement.vdecl.modifier == ast::var_decl::mod_t::con && !statement.vdecl.e )
                    error( statement.src_loc, "const declaration requires an initializer" );
                if ( statement.vdecl.e )
                    resolve_expression( *statement.vdecl.e, parent_scope );
                result.bindings.at( binding.value ).initialized = statement.vdecl.e.has_value();
                return;
            }

            case ast::stmt::ret:
                if ( !in_function( parent_scope ) )
                    error( statement.src_loc, "return used outside a function" );
                if ( statement.e ) resolve_expression( *statement.e, parent_scope );
                return;

            case ast::stmt::brk:
            case ast::stmt::cont:
                if ( !in_loop( parent_scope ) )
                    error( statement.src_loc, statement.cat == ast::stmt::brk ? "break" : "continue", " used outside a loop" );
                return;

            default:
                if ( statement.e ) resolve_expression( *statement.e, parent_scope );
                for ( const auto& substatement : statement.subs )
                    resolve_statement( substatement, current );
                return;
        }
    }

    void declare_functions( const ast::program& program, scope_id global )
    {
        for ( const auto& item : program.toplevel_items )
        {
            const auto* declaration = std::get_if< ast::fn_decl >( &item );
            if ( !declaration ) continue;

            const auto name = intern( declaration->name );
            if ( global_functions.contains( name ) )
                error( declaration->src_loc, "redeclaration of function: ", declaration->name );
            function_id id{ static_cast< uint32_t >( result.functions.size() ) };
            const auto function_scope = add_scope( scope::kind::function, global );
            result.functions.push_back( function{ id, name, declaration, function_scope, declaration->params.size() } );
            result.function_declarations.emplace( declaration, id );
            global_functions.emplace( name, id );
        }
    }

    void build_function_scopes( const function& function )
    {
        for ( const auto& parameter : function.declaration->params )
            declare( parameter, function.scope, true );
        for ( const auto& statement : function.declaration->body )
            build_statement( statement, function.scope );
    }

    analysis_result run( const ast::program& program )
    {
        const auto global = add_scope( scope::kind::global, {} );
        declare_functions( program, global );

        for ( const auto& function : result.functions )
            build_function_scopes( function );
        for ( const auto& function : result.functions )
            for ( const auto& statement : function.declaration->body )
                resolve_statement( statement, function.scope );

        for ( const auto& item : program.toplevel_items )
            if ( const auto* statement = std::get_if< ast::stmt >( &item ) )
            {
                build_statement( *statement, global );
                resolve_statement( *statement, global );
            }

        return std::move( result );
    }
};

}
