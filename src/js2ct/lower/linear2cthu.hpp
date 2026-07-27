#pragma once

#include "../ir/cthu.hpp"
#include "../ir/linear.hpp"
#include "../printer/pretty_printer.hpp"

namespace qthu::js2ct::cthu
{

struct structure_builder
{
    std::string name;
    lin::function& fn;
    cthu::structure* curr_struct = nullptr;

    std::vector< std::string > print_args( const std::vector< lin::argument >& args )
    {
        std::vector< std::string > res{};
        print::pretty_printer pp{};
        for ( auto& arg : args )
        {
            std::ostringstream os;
            pp.print_lin_argument( os, arg );
            res.push_back( os.str() );
        }

        return res;
    }

    void emit(  cthu::function& fn,
                std::string structure,
                std::string op,
                std::vector< lin::argument > in,
                std::vector< lin::argument > out )
    {
        fn.body.push_back( cthu::insn{ structure, op, std::move( print_args( in ) ), std::move( print_args( out ) ) } );
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

    void lower_fn( std::string name, std::vector< lin::instr >& ins )
    {
        cthu::function curr_fn{};

        for ( auto& i : ins )
        {
            if ( auto* cd = std::get_if< lin::cons_data >( &i.data ) )
            {
                if ( auto* int_val = std::get_if< uint64_t >( &cd->c ) )
                    emit( curr_fn, "jsvalue", "cons_" + std::to_string( *int_val ), {}, cd->target );
                else
                    emit( curr_fn, "jsvalue", "cons_" + std::get< bool >( cd->c ) ? "true" : "false", {}, cd->target );
            }

            else if ( auto* u = std::get_if< lin::unary_data >( &i.data ) )
                emit( curr_fn, "jsvalue", op_to_str( u->op ), { u->arg1 }, { u->target } );
            
            else if ( auto* b = std::get_if< lin::binary_data >( &i.data ) )
                emit( curr_fn, "jsvalue", op_to_str( b->op ), { b->arg1, b->arg2 }, { b->target } );

            else if ( auto* c = std::get_if< lin::copy_data >( &i.data ) )
                emit( curr_fn, "jsvalue", "dup", { c->arg1 }, { c->target } );

            else if ( auto* dd = std::get_if< lin::dup_data >( &i.data ) )
                emit( curr_fn, "jsvalue", "dup", { dd->arg1 }, { dd->first, dd->second } );

            else if ( auto* r = std::get_if< lin::ret_data >( &i.data ) )
            {
                // dup the condition into cmp1 and cmp2
                // negate cmp2 into cmp
                // create then, else, frame functions,
                // fsig opt cmp1 then_ref -> alt1
                // fsig opt cmp2 else_ref -> alt2
                // fsig opt alt1 alt2 frame -> cont
                // fsig call cont 
            }

            else if ( auto* id = std::get_if< lin::if_data >( &i.data ) )
            {
            }

            else
                assert( false && "unimplemented" );

            // else if ( auto* c = std::get_if< lin::call_data >( &i.data ) )
            // else if ( auto* l = std::get_if< lin::loop_data >( &i.data ) )
            // else if ( std::get_if< lin::brk_data >( &i.data ) )
            // else if ( std::get_if< lin::cont_data >( &i.data ) )
        }        

        curr_struct->functions[ name ] = std::move( curr_fn );
    }

    cthu::structure lower()
    {
        cthu::structure res{ .id = name };
        curr_struct = &res;
        lower_fn( "run", fn.body );
        return *curr_struct;
    }

};

struct lowerer
{
    cthu::module lower( lin::program& prog )
    {
        cthu::module mod{};
        for ( int i = 0; i < prog.functions.size(); ++i )
        {
            std::string name = i == 0 ? "main" : "f" + std::to_string( i );
            structure_builder sb{ name, prog.functions[ i ] };
            mod.structures.push_back( std::move( sb.lower() ) );
        }
        return mod;
    }
};

}
