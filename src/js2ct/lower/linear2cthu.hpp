#pragma once

#include "../ir/cthu.hpp"
#include "../ir/linear.hpp"
#include "../printer/pretty_printer.hpp"
#include "../sema/analysis.hpp"

namespace qthu::js2ct::cthu
{

struct structure_builder
{
    std::string struct_name;
    lin::function& fn;
    sema::analysis_result& sema;
    structure* curr_struct = nullptr;
    uint32_t next_val = 1;

    std::vector< std::string > vals2str( const std::vector< lin::value >& vals )
    {
        std::vector< std::string > res{};
        print::pretty_printer pp{};
        for ( auto& val : vals )
        {
            std::ostringstream os;
            pp.print_lin_value( os, val );
            res.push_back( std::move( os.str() ) );
        }
        return res;
    }

    std::vector< std::string > args2str( const std::vector< lin::argument >& args )
    {
        std::vector< std::string > res{};
        print::pretty_printer pp{};
        for ( auto& arg : args )
        {
            std::ostringstream os;
            pp.print_lin_argument( os, arg );
            res.push_back( std::move( os.str() ) );
        }

        return res;
    }

    void emit(  function& fn,
                std::string structure,
                std::string op,
                std::vector< std::string > in,
                std::vector< std::string > out )
    {
        fn.body.push_back( insn{ structure, op, std::move( in ), std::move( out ) } );
    }

    void emit(  function& fn,
                std::string structure,
                std::string op,
                std::vector< lin::argument > in,
                std::vector< lin::argument > out )
    {
        fn.body.push_back( insn{ structure, op, std::move( args2str( in ) ), std::move( args2str( out ) ) } );
    }

    std::string op_to_str( op_kind op )
    {
        switch ( op )
        {
            case ADD:   return "add";
            case SUB:   return "sub";
            case MUL:   return "mul";
            case DIV:   return "div";
            case MOD:   return "rem";
            case EQ:    return "eq?";
            case NEQ:   return "ne?";
            case LT:    return "lt?";
            case LEQ:   return "le?";
            case GT:    return "gt?";
            case GEQ:   return "ge?";
            default:
                assert( false && "unimplemented" );
        }

        assert( false && "unimplemented" );
        return "";
    }

    std::string compact_run( char c, size_t k )
    {
        if ( k == 0 )
            return "";
        if ( k == 1 )
            return std::string( 1, c );
        return std::string( 1, c ) + std::to_string( k );
    }

    std::string call_signature_name( size_t n )
    {
        return "f_" + compact_run( 'j', n ) + "_j";
    }

    // Pack `values` into a single array (cons_arr + positional set per element),
    // returning the name holding the final array. Needed because a cthu function/
    // call can only ever propagate one output (QuickJS functions return a single
    // value) — this is how a loop's N live bindings travel through opt/join/call
    // as one value instead of N separately-declared (and silently truncated) outputs.
    std::string pack_values( function& fn, const std::vector< std::string >& values )
    {
        std::string cur = fresh_val( "arr" );
        emit( fn, "jsvalue", "cons_arr", {}, { cur } );
        for ( size_t k = 0; k < values.size(); ++k )
        {
            std::string key = fresh_val( "k" );
            emit( fn, "jsvalue", "cons_" + std::to_string( k ), {}, { key } );
            std::string next = fresh_val( "arr" );
            emit( fn, "jsvalue", "set", { cur, key, values[ k ] }, { next } );
            cur = next;
        }
        return cur;
    }

    // Inverse of pack_values: unpack `packed` positionally into `targets`,
    // dup'ing the array for every element but the last (get consumes its array
    // operand, mirroring qjs_val_get's obj x key -> value linear discipline).
    void unpack_values( function& fn, const std::string& packed, const std::vector< std::string >& targets )
    {
        std::string cur = packed;
        for ( size_t k = 0; k < targets.size(); ++k )
        {
            std::string src = cur;
            if ( k + 1 < targets.size() )
            {
                std::string a = fresh_val( "u" );
                std::string b = fresh_val( "u" );
                emit( fn, "jsvalue", "dup", { cur }, { a, b } );
                src = a;
                cur = b;
            }
            std::string key = fresh_val( "k" );
            emit( fn, "jsvalue", "cons_" + std::to_string( k ), {}, { key } );
            emit( fn, "jsvalue", "get", { src, key }, { targets[ k ] } );
        }
    }

    std::string fresh_val( std::string prefix )
    {
        return prefix + std::to_string( next_val++ );
    } 

    function create_frame( const std::vector< std::string >& params, std::string fsig )
    {
        function res{};
        res.out = { "out" };
        std::vector< std::string > param_names;
        param_names.push_back( "A" );
        param_names.push_back( "B" );
        for ( auto& s : params )
            param_names.push_back( s );
        
        res.in = std::move( param_names );
        std::vector< std::string > dup_first{ "A" };
        std::vector< std::string > dup_second{ "B" };

        for ( int i = 2; i < res.in.size(); ++ i )
        {
            std::string fst = res.in[ i ] + "_1" ;
            std::string snd = res.in[ i ] + std::string( "_2" );
            dup_first.push_back( fst );
            dup_second.push_back( snd );
            emit( res, "jsvalue", "dup", { res.in[ i ] }, { fst, snd } );
        }
        
        emit( res, fsig, "call", std::move( dup_first ), { "out1" } );
        emit( res, fsig, "call", std::move( dup_second ), { "out2" } );
        emit( res, "jsvalue", "join", { "out1", "out2" }, { "out" } );
        return res;
    }

    void lower_fn( std::string name, std::vector< lin::instr >& ins, std::vector< cthu::insn > extra = {} )
    {
        function curr_fn{};

        for ( auto& i : ins )
        {
            if ( auto* cd = std::get_if< lin::cons_data >( &i.data ) )
            {
                if ( auto* int_val = std::get_if< uint64_t >( &cd->c ) )
                    emit( curr_fn, "jsvalue", "cons_" + std::to_string( *int_val ), {}, { cd->target } );
                else
                    emit( curr_fn, "jsvalue", "cons_" + std::string( std::get< bool >( cd->c ) ? "true" : "false" ), {}, { cd->target } );
            }

            else if ( auto* u = std::get_if< lin::unary_data >( &i.data ) )
                emit( curr_fn, "jsvalue", op_to_str( u->op ), { u->arg1 }, { u->target } );
            
            else if ( auto* b = std::get_if< lin::binary_data >( &i.data ) )
                emit( curr_fn, "jsvalue", op_to_str( b->op ), { b->arg1, b->arg2 }, { b->target } );

            else if ( auto* c = std::get_if< lin::copy_data >( &i.data ) )
                emit( curr_fn, "jsvalue", "copy", { c->arg1 }, { c->target } );

            else if ( auto* dd = std::get_if< lin::dup_data >( &i.data ) )
                emit( curr_fn, "jsvalue", "dup", { dd->arg1 }, { dd->first, dd->second } );

            else if ( auto* dr = std::get_if< lin::drop_data >( &i.data ) )
                emit( curr_fn, "jsvalue", "drop", {}, { dr->target } );

            else if ( auto* id = std::get_if< lin::if_data >( &i.data ) )
            {
                std::string cmp1 = fresh_val( "cmp" );
                std::string cmp2 = fresh_val( "cmp" );
                std::string cmp3 = fresh_val( "cmp" );

                emit( curr_fn, "jsvalue", "dup", args2str( { id->cond } ), { cmp1, cmp2 } );
                emit( curr_fn, "jsvalue", "not", { cmp2 }, { cmp3 } );

                std::string then_name = fresh_val( "then" );
                std::string else_name = fresh_val( "else" );
                std::string frame_name = fresh_val( "frame" );
                                
                lower_fn( then_name, id->then_body );
                lower_fn( else_name, id->else_body );

                std::vector< std::string > params = vals2str( id->params );
                std::string fsig = call_signature_name( params.size() );

                curr_struct->functions[ then_name ].in = params;
                curr_struct->functions[ then_name ].out = { "out" };
                curr_struct->functions[ else_name ].in = params;
                curr_struct->functions[ else_name ].out = { "out" };

                function frame_fn = create_frame( params, fsig );
                curr_struct->functions[ frame_name ] = std::move( frame_fn );

                emit( curr_fn, struct_name, "", { then_name }, { then_name } );
                emit( curr_fn, struct_name, "", { else_name }, { else_name } );
                
                std::string alt1_name = fresh_val( "alt" );
                std::string alt2_name = fresh_val( "alt" );
                emit( curr_fn, fsig, "opt", { cmp1, then_name }, { alt1_name } );
                emit( curr_fn, fsig, "opt", { cmp3, else_name }, { alt2_name } );

                std::string cont = fresh_val( "cont" );
                emit( curr_fn, fsig, "join", { alt1_name, alt2_name, frame_name }, { cont } );

                std::vector call_args{ cont };
                for ( auto& p : params )
                    call_args.push_back( std::move( p ) );
                emit( curr_fn, fsig, "call", call_args, { "out" } );
            }

            // todo: we should probably stop codegen of curr_fn after hitting return because everything that follows is dead code,
            else if ( auto* r = std::get_if< lin::ret_data >( &i.data ) )
            {
                // what to do with functions that don't "return" anything ? 
                if ( r->arg )
                    emit( curr_fn, "jsvalue", "move", args2str( { r->arg.value() } ), { "out" } );
            }
            else if ( auto* dr = std::get_if< lin::drop_data >( &i.data ) ) {}
            else if ( auto* c = std::get_if< lin::call_data >( &i.data ) )
            {
                std::string callee_struct = sema.function_name( c->callee );
                std::string f_ref = fresh_val( "f_ref" );
                emit( curr_fn, callee_struct, "run", {}, { f_ref } );

                std::string fsig = call_signature_name( c->args.size() );
                std::vector< std::string > call_args{ f_ref };
                for ( auto& a : args2str( c->args ) )
                    call_args.push_back( std::move( a ) );

                emit( curr_fn, fsig, "call", call_args, vals2str( { c->target } ) );
            }
            else if ( auto* ld = std::get_if< lin::loop_data >( &i.data ) )
            {
                std::vector< std::string > params = vals2str( ld->params );
                std::vector< std::string > outs = vals2str( ld->outputs );
                std::string fsig = call_signature_name( params.size() );

                std::string loop_name = fresh_val( "loop" );
                std::string cont_name = fresh_val( "loopbody" );
                std::string exit_name = fresh_val( "loopexit" );
                std::string frame_name = fresh_val( "loopframe" );

                // Every closure below has exactly one output: a call can only ever
                // propagate insn.slots_out[0] (QuickJS functions return a single
                // value), so a loop's N live bindings travel packed into one array
                // (pack_values) and are unpacked (unpack_values) only where an
                // individual binding is actually needed again — after the outer call.

                // exit branch: pack the loop's live params straight through as the result.
                {
                    cthu::function exit_fn{};
                    exit_fn.in = params;
                    std::string packed = pack_values( exit_fn, params );
                    exit_fn.out = { packed };
                    curr_struct->functions[ exit_name ] = std::move( exit_fn );
                }

                // continue branch: run one iteration of the body, then tail-recurse
                // into loop_name (self-reference by name) with the updated values.
                {
                    std::string self_ref = fresh_val( "self" );
                    std::vector rec_args{ self_ref };
                    for ( auto& p : vals2str( ld->next_params ) )
                        rec_args.push_back( p );

                    std::vector< insn > extra;
                    extra.push_back( insn{ struct_name, loop_name, {}, { self_ref } } );
                    extra.push_back( insn{ fsig, "call", rec_args, { "packed" } } );

                    lower_fn( cont_name, ld->body, extra );
                    curr_struct->functions[ cont_name ].in = params;
                    curr_struct->functions[ cont_name ].out = { "packed" };
                }

                // frame: dup each param for both branches, call both (one always
                // hits the opt'd-out "bot" closure), join the two packed results.
                {
                    function frame_fn{};
                    std::vector< std::string > frame_in{ "A", "B" };
                    for ( auto& p : params )
                        frame_in.push_back( p );
                    frame_fn.in = frame_in;

                    std::vector< std::string > dup_first{ "A" }, dup_second{ "B" };
                    for ( size_t k = 2; k < frame_in.size(); ++k )
                    {
                        std::string fst = frame_in[ k ] + "_1", snd = frame_in[ k ] + "_2";
                        dup_first.push_back( fst );
                        dup_second.push_back( snd );
                        frame_fn.body.push_back( insn{ "jsvalue", "dup", { frame_in[ k ] }, { fst, snd } } );
                    }

                    std::string packed1 = fresh_val( "p1" ), packed2 = fresh_val( "p2" );
                    frame_fn.body.push_back( insn{ fsig, "call", dup_first, { packed1 } } );
                    frame_fn.body.push_back( insn{ fsig, "call", dup_second, { packed2 } } );
                    std::string packed = fresh_val( "packed" );
                    frame_fn.body.push_back( insn{ "jsvalue", "join", { packed1, packed2 }, { packed } } );
                    frame_fn.out = { packed };

                    curr_struct->functions[ frame_name ] = std::move( frame_fn );
                }

                // loop_name: the self-recursive dispatcher. Re-runs cond_body and
                // the opt/join/call dispatch on *every* call, initial or recursive.
                {
                    std::string cmp1 = fresh_val( "cmp" ), cmp2 = fresh_val( "cmp" ), cmp3 = fresh_val( "cmp" );
                    std::string cont_ref = fresh_val( "ref" ), exit_ref = fresh_val( "ref" ), frame_ref = fresh_val( "ref" );
                    std::string alt1 = fresh_val( "alt" ), alt2 = fresh_val( "alt" ), joined = fresh_val( "cont" );

                    std::vector< insn > extra;
                    extra.push_back( insn{ "jsvalue", "dup", args2str( { ld->cond } ), { cmp1, cmp2 } } );
                    extra.push_back( insn{ "jsvalue", "not", { cmp2 }, { cmp3 } } );
                    extra.push_back( insn{ struct_name, cont_name, {}, { cont_ref } } );
                    extra.push_back( insn{ struct_name, exit_name, {}, { exit_ref } } );
                    extra.push_back( insn{ struct_name, frame_name, {}, { frame_ref } } );
                    extra.push_back( insn{ fsig, "opt", { cmp1, cont_ref }, { alt1 } } );
                    extra.push_back( insn{ fsig, "opt", { cmp3, exit_ref }, { alt2 } } );
                    extra.push_back( insn{ fsig, "join", { alt1, alt2, frame_ref }, { joined } } );

                    // Use dispatch_args, not params: cond_body (which just
                    // ran, right above) may have already consumed some of
                    // loop_name's own declared "in" names via dup.
                    std::vector call_args{ joined };
                    for ( auto& p : vals2str( ld->dispatch_args ) )
                        call_args.push_back( p );
                    extra.push_back( insn{ fsig, "call", call_args, { "packed" } } );

                    lower_fn( loop_name, ld->cond_body, extra );
                    curr_struct->functions[ loop_name ].in = params;
                    curr_struct->functions[ loop_name ].out = { "packed" };
                }

                // back in the enclosing function: reference loop_name, call it with
                // the current live params, then unpack the single packed result
                // back into the individual bindings the rest of the function expects.
                std::string loop_ref = fresh_val( "ref" );
                emit( curr_fn, struct_name, loop_name, {}, { loop_ref } );
                std::vector outer_call_args{ loop_ref };
                for ( auto& p : params )
                    outer_call_args.push_back( p );
                std::string packed_result = fresh_val( "packed" );
                emit( curr_fn, fsig, "call", outer_call_args, { packed_result } );
                unpack_values( curr_fn, packed_result, outs );
            }
            else if ( std::get_if< lin::brk_data >( &i.data ) ) {}
            else if ( std::get_if< lin::cont_data >( &i.data ) ) {}
            else
                assert( false && "unimplemented" );
        }

        for ( auto& e : extra )
            curr_fn.body.push_back( std::move( e ) );

        curr_struct->functions[ name ] = std::move( curr_fn );
    }

    structure lower()
    {
        structure res{ .id = struct_name };
        curr_struct = &res;
        lower_fn( "run", fn.body );
        curr_struct->functions[ "run" ].in = vals2str( fn.params );

        // ret_data (a direct `return`) and if_data/loop_data's own final tail
        // call (a `return` inside a branch) both write their result into a
        // slot literally named "out" -- but only when the function actually
        // returns something (the top-level script never does). Detect that
        // by scanning the built body rather than threading a flag through
        // every lower_fn case, so a void function's "run" keeps an empty
        // .out (matching reader.cpp: no declared .out means no return value).
        bool produces_out = false;
        for ( auto& insn : curr_struct->functions[ "run" ].body )
            for ( auto& o : insn.out )
                if ( o == "out" )
                    produces_out = true;

        if ( produces_out )
            curr_struct->functions[ "run" ].out = { "out" };

        return *curr_struct;
    }
};

struct lowerer
{
    sema::analysis_result& sema;

    cthu::module lower( lin::program& prog )
    {
        cthu::module mod{};
        for ( int i = 0; i < prog.functions.size(); ++i )
        {
            std::string struct_name = i == 0 ? "main" : sema.function_name( prog.functions[ i ].name );
            structure_builder sb{ struct_name, prog.functions[ i ], sema };
            mod.structures.push_back( std::move( sb.lower() ) );
        }
        return mod;
    }
};

}
