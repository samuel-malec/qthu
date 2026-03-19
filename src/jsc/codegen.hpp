#pragma once

#include <algorithm>
#include <cstdint>
#include <format>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "ast.hpp"
#include "symtab.hpp"
#include "../asm/asmbuilder.hpp"
#include "../asm/op_builders.hpp"

namespace jsc
{

using asbuilder = qthu::asmbuilder;

enum class storage
{
    arg,
    local,
    function,
    invalid,
};

struct resolved_symbol
{
    storage where = storage::invalid;
    uint16_t index = 0;
};

struct codegen_context
{
    const symtab& global;
    const loctab& local;
    const std::vector< uint16_t >& non_main_functions;

    std::string_view function_name;
    uint16_t function_index = 0;
    uint16_t arg_index_bias = 0;
    std::size_t label_counter = 0;

    int current_stack = 0;
    int max_stack = 0;

    resolved_symbol lookup( std::string_view name ) const
    {
        auto it_local = local.locals.find( std::string( name ) );
        if ( it_local != local.locals.end() )
            return { storage::local, it_local->second };

        auto it_arg = local.args.find( std::string( name ) );
        if ( it_arg != local.args.end() )
            return { storage::arg, it_arg->second };

        auto it_fn = global.funcs.find( std::string( name ) );
        if ( it_fn != global.funcs.end() )
            return { storage::function, it_fn->second };

        return { storage::invalid, 0 };
    }

    uint16_t env_slot_for( uint16_t fn_index ) const
    {
        for ( std::size_t i = 0; i < non_main_functions.size(); ++i )
            if ( non_main_functions[ i ] == fn_index )
                return static_cast< uint16_t >( 1 + i );
        throw std::runtime_error( "Function not present in non-main closure environment" );
    }

    void push( int n = 1 )
    {
        current_stack += n;
        max_stack = std::max( max_stack, current_stack );
    }

    void pop( int n = 1 )
    {
        current_stack -= n;
        if ( current_stack < 0 )
            throw std::runtime_error( "Internal stack-tracking underflow" );
    }

    std::string make_label( std::string_view prefix )
    {
        return std::format( "{}_{}", std::string( prefix ), label_counter++ );
    }
};

inline void emit_expr( asbuilder& builder, codegen_context& ctx, const expr& e );

inline void emit_identifier_load( asbuilder& builder, codegen_context& ctx, const expr& e )
{
    resolved_symbol symb = ctx.lookup( e.lit );
    switch ( symb.where )
    {
        case storage::local:
            builder.add_instr( qthu::as::get_loc_( symb.index ) );
            ctx.push();
            return;
        case storage::arg:
            builder.add_instr( qthu::as::get_arg_( symb.index + ctx.arg_index_bias ) );
            ctx.push();
            return;
        default:
            throw std::runtime_error( std::format( "Invalid identifier '{}'", std::string( e.lit ) ) );
    }
}

inline void emit_binary( asbuilder& builder, codegen_context& ctx, const expr& e )
{
    emit_expr( builder, ctx, e.subs[ 0 ] );
    emit_expr( builder, ctx, e.subs[ 1 ] );

    const auto& op = e.op;

    if ( op == "+" )
        builder.add_instr( qthu::as::add_() );
    else if ( op == "-" )
        builder.add_instr( qthu::as::sub_() );
    else if ( op == "*" )
        builder.add_instr( qthu::as::mul_() );
    else if ( op == "/" )
        builder.add_instr( qthu::as::div_() );
    else if ( op == "%" )
        builder.add_instr( qthu::as::mod_() );
    else if ( op == "==" )
        builder.add_instr( qthu::as::eq_() );
    else if ( op == "!=" )
        builder.add_instr( qthu::as::neq_() );
    else if ( op == "<" )
        builder.add_instr( qthu::as::lt_() );
    else if ( op == "<=" )
        builder.add_instr( qthu::as::lte_() );
    else if ( op == ">" )
        builder.add_instr( qthu::as::gt_() );
    else if ( op == ">=" )
        builder.add_instr( qthu::as::gte_() );
    else if ( op == "&&" )
        builder.add_instr( qthu::as::and_() );
    else if ( op == "||" )
        builder.add_instr( qthu::as::or_() );
    else
        throw std::runtime_error( std::format( "Unsupported binary operator '{}'", op ) );

    ctx.pop();
}

inline void emit_assignment( asbuilder& builder, codegen_context& ctx, const expr& e )
{
    if ( e.subs.size() != 2 || e.subs[ 0 ].kind != expr::kind_t::identifier )
        throw std::runtime_error( "Only simple identifier assignment is supported" );

    const expr& lhs = e.subs[ 0 ];
    const expr& rhs = e.subs[ 1 ];

    emit_expr( builder, ctx, rhs );

    resolved_symbol symb = ctx.lookup( lhs.lit );
    switch ( symb.where )
    {
        case storage::local:
            builder.add_instr( qthu::as::set_loc_( symb.index ) );
            return;
        case storage::arg:
            builder.add_instr( qthu::as::set_arg_( symb.index + ctx.arg_index_bias ) );
            return;
        default:
            throw std::runtime_error( std::format( "Invalid identifier '{}'", std::string( lhs.lit ) ) );
    }
}

inline void emit_call( asbuilder& builder, codegen_context& ctx, const expr& e )
{
    resolved_symbol symb = ctx.lookup( e.lit );
    if ( symb.where != storage::function )
        throw std::runtime_error( "Invalid function call" );

    const int env_count = static_cast< int >( ctx.non_main_functions.size() );

    if ( ctx.function_name == "main" )
    {
        if ( symb.index == 0 )
            throw std::runtime_error( "Main cannot call itself with current cpool layout" );

        const uint32_t cpool_index = static_cast< uint32_t >( symb.index - 1 );
        builder.add_instr( qthu::as::fclosure_( cpool_index ) );
        ctx.push();

        // hidden self arg
        builder.add_instr( qthu::as::dup_() );
        ctx.push();

        // hidden environment args (closures for all non-main functions)
        for ( uint16_t fn_idx : ctx.non_main_functions )
        {
            const uint32_t env_cpool_idx = static_cast< uint32_t >( fn_idx - 1 );
            builder.add_instr( qthu::as::fclosure_( env_cpool_idx ) );
            ctx.push();
        }
    }
    else
    {
        // Non-main functions receive hidden env closures as args and never emit fclosure.
        if ( symb.index == 0 )
            throw std::runtime_error( "Non-main functions cannot call main with current calling convention" );

        const uint16_t callee_slot = ctx.env_slot_for( symb.index );
        builder.add_instr( qthu::as::get_arg_( callee_slot ) );
        ctx.push();

        // hidden self arg for callee
        builder.add_instr( qthu::as::get_arg_( callee_slot ) );
        ctx.push();

        // pass hidden env through unchanged
        for ( uint16_t fn_idx : ctx.non_main_functions )
        {
            const uint16_t slot = ctx.env_slot_for( fn_idx );
            builder.add_instr( qthu::as::get_arg_( slot ) );
            ctx.push();
        }
    }

    for ( const expr& ex : e.subs )
        emit_expr( builder, ctx, ex );

    const int arg_count = 1 + env_count + static_cast< int >( e.subs.size() );
    builder.add_instr( qthu::as::call_( arg_count ) );
    ctx.pop( arg_count + 1 );
    ctx.push();
}

inline void emit_expr( asbuilder& builder, codegen_context& ctx, const expr& e )
{
    using kind = expr::kind_t;
    switch ( e.kind )
    {
        case kind::immediate:
            builder.add_instr( qthu::as::push_i32_( e.value ) );
            ctx.push();
            return;
        case kind::identifier:
            emit_identifier_load( builder, ctx, e );
            return;
        case kind::assignment:
            emit_assignment( builder, ctx, e );
            return;
        case kind::binary:
            if ( e.subs.size() != 2 )
                throw std::runtime_error( "Binary operation needs exactly two operands" );
            emit_binary( builder, ctx, e );
            return;
        case kind::unary:
            if ( e.subs.size() != 1 )
                throw std::runtime_error( "Unary operation needs exactly one operand" );
            if ( e.op == "+" )
            {
                emit_expr( builder, ctx, e.subs[ 0 ] );
                return;
            }
            if ( e.op == "-" )
            {
                emit_expr( builder, ctx, e.subs[ 0 ] );
                builder.add_instr( qthu::as::neg_() );
                return;
            }
            throw std::runtime_error( std::format( "Unsupported unary operation: '{}'", e.op ) );
        case kind::call:
            emit_call( builder, ctx, e );
            return;
    }
}

inline void emit_stmt( asbuilder& builder, codegen_context& ctx, const stmt& s )
{
    using kind = stmt::kind_t;
    switch ( s.kind )
    {
        case kind::expression:
            emit_expr( builder, ctx, s.value );
            builder.add_instr( qthu::as::drop_() );
            ctx.pop();
            return;

        case kind::ret:
            emit_expr( builder, ctx, s.value );
            builder.add_instr( qthu::as::return_() );
            ctx.pop();
            return;

        case kind::if_stmt:
        {
            const std::string else_label = ctx.make_label( "else" );
            const std::string endif_label = ctx.make_label( "endif" );

            emit_expr( builder, ctx, s.condition );
            builder.add_instr( qthu::as::if_false_( s.else_body.empty() ? endif_label : else_label ) );
            ctx.pop();

            for ( const auto& ts : s.then_body )
                emit_stmt( builder, ctx, ts );

            if ( !s.else_body.empty() )
            {
                builder.add_instr( qthu::as::goto_( endif_label ) );
                builder.add_label( else_label );
                for ( const auto& es : s.else_body )
                    emit_stmt( builder, ctx, es );
            }

            builder.add_label( endif_label );
            return;
        }
    }
}

inline void generate( asbuilder& builder, const program& prog )
{
    const symtab st = populate( prog );

    std::vector< uint16_t > non_main_functions;
    for ( const auto& [ _, idx ] : st.funcs )
        if ( idx != 0 )
            non_main_functions.push_back( idx );
    std::sort( non_main_functions.begin(), non_main_functions.end() );

    auto emit_function = [&]( const function_decl& fnd ) {
        auto it = st.loctabs.find( std::string( fnd.name ) );
        auto fit = st.funcs.find( std::string( fnd.name ) );

        const auto& lt = it->second;
        const bool is_main = ( fnd.name == "main" );
        const uint16_t arg_bias = is_main ? 0 : static_cast< uint16_t >( 1 + non_main_functions.size() );
        const uint16_t emitted_arg_count = static_cast< uint16_t >( fnd.params.size() + arg_bias );

        // stack_size = 100 placeholder
        builder.add_function( fnd.name, emitted_arg_count, lt.local_count, 100 );

        codegen_context ctx{ st, lt, non_main_functions, fnd.name, fit->second, arg_bias };
        for ( const stmt& s : fnd.body )
            emit_stmt( builder, ctx, s );

        // do we want to throw on function that do not return anything ? 
        if ( fnd.body.empty() || fnd.body.back().kind != stmt::kind_t::ret )
            builder.add_instr( qthu::as::return_undef_() );

        builder.current->stack_size = ctx.max_stack;
    };

    for ( const function_decl& fnd : prog.functions )
        if ( fnd.name == "main" )
            emit_function( fnd );

    for ( const function_decl& fnd : prog.functions )
    {
        if ( fnd.name == "main" )
            continue;
        emit_function( fnd );
    }
}

} // namespace jsc
