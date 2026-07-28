#pragma once

#include "../ir/cthu.hpp"
#include "../ir/linear.hpp"
#include "../printer/pretty_printer.hpp"

namespace qthu::js2ct::cthu
{

struct struct_context
{
    uint32_t next_fn = 1;
    uint32_t next_cmp = 1;
};

struct structure_builder
{
    std::string name;
    lin::function& fn;
    cthu::structure* curr_struct = nullptr;
    struct_context ctx;

    std::vector< std::string > args2str( const std::vector< lin::argument >& args )
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
                const std::vector< std::string >& in,
                const std::vector< std::string >& out )
    {
        fn.body.push_back( cthu::insn{ structure, op, std::move( in ), std::move( out ) } );
    }

    void emit(  cthu::function& fn,
                std::string structure,
                std::string op,
                const std::vector< lin::argument >& in,
                const std::vector< lin::argument >& out )
    {
        fn.body.push_back( cthu::insn{ structure, op, std::move( args2str( in ) ), std::move( args2str( out ) ) } );
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

    std::string call_signature_name( size_t n )
    {
        return "f_" + std::string( n, 'i' ) + "_i";
    }

    std::string fresh_cmp()
    {
        return "cmp" + ctx.next_cmp++;
    }

    std::string fresh_fn( std::string prefix )
    {
        return prefix + ctx.next_fn++;
    }

    void lower_fn( std::string name, std::vector< lin::instr >& ins )
    {
        cthu::function curr_fn{};

        for ( auto& i : ins )
        {
            if ( auto* cd = std::get_if< lin::cons_data >( &i.data ) )
            {
                if ( auto* int_val = std::get_if< uint64_t >( &cd->c ) )
                    emit( curr_fn, "jsvalue", "cons_" + std::to_string( *int_val ), {}, { cd->target } );
                else
                    emit( curr_fn, "jsvalue", "cons_" + std::get< bool >( cd->c ) ? "true" : "false", {}, { cd->target } );
            }

            else if ( auto* u = std::get_if< lin::unary_data >( &i.data ) )
                emit( curr_fn, "jsvalue", op_to_str( u->op ), { u->arg1 }, { u->target } );
            
            else if ( auto* b = std::get_if< lin::binary_data >( &i.data ) )
                emit( curr_fn, "jsvalue", op_to_str( b->op ), { b->arg1, b->arg2 }, { b->target } );

            else if ( auto* c = std::get_if< lin::copy_data >( &i.data ) )
                emit( curr_fn, "jsvalue", "dup", { c->arg1 }, { c->target } );

            else if ( auto* dd = std::get_if< lin::dup_data >( &i.data ) )
                emit( curr_fn, "jsvalue", "dup", { dd->arg1 }, { dd->first, dd->second } );
            else if ( auto* id = std::get_if< lin::if_data >( &i.data ) )
            {
                std::string cmp  = fresh_cmp();
                std::string cmp1 = fresh_cmp();
                std::string cmp2 = fresh_cmp();
                std::string cmp3 = fresh_cmp();
                
                emit( curr_fn, "jsvalue", "dup", { cmp }, { cmp1, cmp2 } );
                emit( curr_fn, "not", { cmp2 }, { cmp3 } );

                std::string then_name = fresh_fn( "then" );
                std::string else_name = fresh_fn( "else" );
                std::string frame_name = fresh_fn( "frame" );
                
                lower_fn( then_name, id->then_body );
                // todo: check how this behaves with empty functions
                lower_fn( else_name, id->else_body );

                // TODO: infer the parameter size of then, else
                // call fsig on the sig
                // emit fsig opt cmp1 then_ref -> alt1
                // emit fsig opt cmp2 else_ref -> alt2
                // emit fsig opt alt1 alt2 frame -> cont
                // emit fsig call cont 
            }

            else if ( auto* r = std::get_if< lin::ret_data >( &i.data ) )
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
