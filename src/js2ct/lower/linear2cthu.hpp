#pragma once

#include "../ir/cthu.hpp"
#include "../ir/linear.hpp"
#include "../printer/pretty_printer.hpp"

namespace qthu::js2ct
{

struct ct_builder
{
    lin::function& lin_fn;
    cthu::function cthu_fn;

    void emit( std::string structure,
               std::string op,
               std::vector< lin::argument > in,
               lin::value t )
    {
        std::vector< std::string > in_str{};
        print::pretty_printer pp{};
        for ( auto& arg : in )
        {
            std::ostringstream os;
            pp.print_lin_argument( os, arg );
            in_str.push_back( os.str() );
        }

        std::ostringstream os;
        pp.print_lin_value( os, t );
        cthu_fn.body.push_back( cthu::insn{ structure, op, std::move( in_str ), { os.str() } } );
    }

    std::string op_to_str( op_kind op )
    {
        switch ( op )
        {
            case ADD:   return "add";
            case SUB:   return "sub";
            case MUL:   return "mul";
            case DIV:   return "div";
            case MOD:   return "mod";
            case EQ:    return "eq?";
            case LT:    return "lt?";
            case LEQ:   return "le?";
            case GT:    return "gt?";
            default:
                assert( false && "unimplemented" );
        }

        assert( false && "unimplemented" );
        return "";
    }

    cthu::function lower_fn()
    {
        // todo: add function params...
        for ( auto& i : lin_fn.body )
        {
            if ( auto* cd = std::get_if< lin::cons_data >( &i.data ) )
            {
                if ( auto* int_val = std::get_if< uint64_t >( &cd->c ) )
                    emit( "jsvalue", "cons_" + std::to_string( *int_val ), {}, cd->target );
                else
                    emit( "jsvalue", "cons_" + std::get< bool >( cd->c ) ? "true" : "false", {}, cd->target );
            }

            else if ( auto* u = std::get_if< lin::unary_data >( &i.data ) )
                emit( "jsvalue", op_to_str( u->op ), { u->arg1 }, { u->target } );
            
            else if ( auto* b = std::get_if< lin::binary_data >( &i.data ) )
                emit( "jsvalue", op_to_str( b->op ), { b->arg1, b->arg2 }, { b->target } );

            else if ( auto* c = std::get_if< lin::copy_data >( &i.data ) )
                emit( "jsvalue", "dup", { c->arg1 }, { c->target } );

            else
                assert( false && "unimplemented" );

            // else if ( auto* c = std::get_if< lin::call_data >( &i.data ) )
            // else if ( auto* r = std::get_if< lin::ret_data >( &i.data ) )
            // else if ( auto* id = std::get_if< lin::if_data >( &i.data ) )
            // else if ( auto* l = std::get_if< lin::loop_data >( &i.data ) )
            // else if ( std::get_if< lin::brk_data >( &i.data ) )
            // else if ( std::get_if< lin::cont_data >( &i.data ) )
        }

        return std::move( cthu_fn );
    }
};

void lower_structure( cthu::module& mod, std::string name, lin::function& fn )
{
    cthu::structure st{ .id = name };
    ct_builder cb{ .lin_fn = fn };
    st.functions[ "run" ] = std::move( cb.lower_fn() );
    mod.structures.push_back( std::move( st ) );
}

cthu::module lower_linear( lin::program& prog )
{
    cthu::module mod{};
    
    for ( int i = 0; i < prog.functions.size(); ++i )
    {
        std::string name = "";
        if ( i == 0 )
            name = "main";
        else
            name = "f" + std::to_string( i );

        lower_structure( mod, name, prog.functions[ i ] );
    }

    return mod;
}

}
