#include <iostream>
#include <stdexcept>

#include "reader.hpp"

namespace cthu
{
    diag reader::error( token t, auto... msg )
    {
        auto rv = brq::make_refcount< diag_msg >( t.loc, msg... );
        rv->text << ": " << brq::printable( t.data );
        return rv;
    }

    diag reader::parse()
    {
        while ( !lex.empty() )
        {
            token t = fetch();
            diag err;

            switch ( t.cat )
            {
                case token::eol:
                    break;
                case token::kw_type:
                    err = read_type();
                    break;
                case token::kw_sig:
                    err = read_signature();
                    break;
                case token::kw_struct:
                    err = read_structure();
                    break;
                default:
                    err = error( t, "unexpected token at toplevel" );
                    break;
            }

            if ( err )
                return err;
        }

        return nullptr;
    }

    token reader::peek()
    {
        while ( current.cat == token::invalid )
        {
            while ( !lex.empty() && current.cat == token::invalid )
                lex.next();

            if ( current.cat == token::comment )
                current = {};
        }

        return current;
    }

    token reader::fetch()
    {
        auto rv = peek();
        current = {};
        return rv;
    }

    diag reader::skip( token::cat_t cat )
    {
        while ( peek( cat ) )
            fetch();
        return nullptr;
    }

    diag reader::require( token::cat_t cat, std::string_view data )
    {
        auto tok = fetch();

        if ( tok.cat != cat || ( !data.empty() && tok.data != data ) )
            return error( tok, "expected ", cat, " ", brq::printable( data ) );

        if ( cat == token::eol )
            skip( token::eol );

        return nullptr;
    }

    bool reader::peek( token::cat_t cat, std::string_view data )
    {
        auto t = peek();
        return t.cat == cat && ( data.empty() || t.data == data );
    }

    diag reader::read_name( auto *&out, auto &map )
    {
        auto name = fetch();
        if ( name.cat != token::ident )
            return error( name, "expected an identifier" );

        auto key = prog.get( name.data );
        if ( map.contains( key ) )
            return error( name, name.data, " already defined" );

        out = &map[ key ];
        return nullptr;
    }

    diag reader::read_ident_list( auto &out, auto f, std::string_view delim )
    {
        while ( peek( token::ident ) )
        {
            out.push_back( f( fetch() ) );

            if ( delim.empty() )
                continue;

            if ( peek( token::punct, delim ) )
                fetch();
            else
                break;
        }

        return nullptr;
    }

    diag reader::read_type()
    {
        auto name = fetch();
        auto eol = fetch();

        if ( name.cat != token::ident )
            return error( name, "expected an identifier" );

        if ( eol.cat != token::eol )
            return error( eol, "expected an end of line" );

        if ( prog.has_type( name.data ) )
            return error( name, "type already defined" );

        prog.get_type( name.data );
        return nullptr;
    }

    diag reader::read_sig_args( signature_t &out )
    {
        diag err;

        ( err = require( token::bracket, "[" ) ) ||
        ( err = read_ident_list( out.args, get_atom(), "," ) ) ||
        ( err = require( token::bracket, "]" ) );

        return err;
    }

    diag reader::read_sig_inherit( signature_t &out )
    {
        diag err = require( token::punct, ":" );
        while ( !err && peek( token::ident ) )
        {
            auto key = get_atom()( fetch() );
            std::vector< atom > args;

            ( err = require( token::bracket, "[" ) ) ||
            ( err = read_ident_list( args, get_atom(), "," ) ) ||
            ( err = require( token::bracket, "]" ) );

            if ( !err )
                out.inherits.emplace_back( key, args );

            if ( !err && peek( token::punct, "," ) )
                fetch();
            else
                break;
        }

        return err;
    }

    diag reader::read_sig_type( sig_def_t &def )
    {
        diag err;

        ( err = read_ident_list( def.in, get_atom(), "×" ) ) ||
        ( err = require( token::arrow ) ) ||
        ( err = read_ident_list( def.out, get_atom(), "×" ) );

        return err;
    }

    diag reader::read_sig_def( signature_t &out )
    {
        diag err;
        sig_def_t *def;

        ( err = read_name( def, out.defs ) ) ||
        ( err = require( token::punct, "∷" ) ) ||
        ( err = read_sig_type( *def ) );

        return err;
    }

    diag reader::read_sig_defs( signature_t &out )
    {
        while ( !peek( token::paren, ")" ) )
        {
            while ( peek( token::eol ) )
                fetch();

            if ( peek( token::paren, ")" ) )
                break;

            if ( auto err = read_sig_def( out ) )
                return err;

            if ( auto err = require( token::eol ) )
                return err;
        }

        return nullptr;
    }

    diag reader::read_signature()
    {
        signature_t *out;
        diag err;

        ( err = read_name( out, prog.signatures ) ) ||
        ( err = read_sig_args( *out ) ) ||
        ( peek( token::punct, ":" ) && ( err = read_sig_inherit( *out ) ) ) ||
        ( err = require( token::eol ) ) ||
        ( err = require( token::paren, "(" ) ) ||
        ( err = skip( token::eol ) ) ||
        ( err = read_sig_defs( *out ) ) ||
        ( err = require( token::paren, ")" ) );

        return err;
    }

    diag reader::read_function( function_t &fn )
    {
        std::vector< atom > in;
        std::vector< atom > out;

        if ( auto err = read_ident_list( in, get_atom(), "" ) )
            return err;

        if ( peek( token::arrow ) )
        {
            fetch();
            if ( auto err = read_ident_list( out, get_atom(), "" ) )
                return err;
        }

        for ( auto a : in )
            fn.in.push_back( a.index );

        for ( auto a : out )
            fn.out.push_back( a.index );

        skip( token::eol );

        if ( auto err = require( token::paren, "(" ) )
            return err;

        while ( true )
        {
            while ( peek( token::eol ) )
                fetch();

            if ( peek( token::paren, ")" ) )
            {
                fetch();
                break;
            }

            auto s = fetch();
            auto o = fetch();

            if ( s.cat != token::ident || o.cat != token::ident )
                return error( s, "expected instruction as: structure operation in... → out..." );

            insn_t insn;
            insn.structure = prog.get( s.data );
            insn.operation = prog.get( o.data );

            std::vector< atom > in_args;
            if ( auto err = read_ident_list( in_args, get_atom(), "" ) )
                return err;

            std::vector< atom > out_args;
            if ( peek( token::arrow ) )
            {
                fetch();
                if ( auto err = read_ident_list( out_args, get_atom(), "" ) )
                    return err;

                if ( out_args.empty() )
                    return error( o, "operation must have at least one output parameter" );
            }
            else
            {
                if ( in_args.empty() )
                    return error( o, "operation must have at least one input or output parameter" );
            }

            insn.args.insert( insn.args.end(), in_args.begin(), in_args.end() );
            insn.args.insert( insn.args.end(), out_args.begin(), out_args.end() );

            if ( auto err = require( token::eol ) )
                return err;

            fn.body.push_back( std::move( insn ) );
        }

        return nullptr;
    }

    diag reader::read_struct_sigs( structure_t &out )
    {
        if ( auto err = require( token::punct, ":" ) )
            return err;

        while ( true )
        {
            auto sig = fetch();
            if ( sig.cat != token::ident )
                return error( sig, "expected signature name" );

            if ( !prog.has_signature( sig.data ) )
                return error( sig, "unknown signature" );

            sig_instance_t inst;
            inst.signature = prog.get( sig.data ).index;

            if ( auto err = require( token::bracket, "[" ) )
                return err;
            
            std::vector< atom > args;
            if ( auto err = read_ident_list( args, get_atom(), "," ) )
                return err;

            if ( auto err = require( token::bracket, "]" ) )
                return err;
            
            for ( auto a : args )
                inst.args.push_back( a.index );

            out.signatures.push_back( std::move( inst ) );

            if ( peek( token::punct, "," ) )
                fetch();
            else
                break;
        }

        return nullptr;
    }

    diag reader::read_struct_defs( structure_t &out )
    {
        diag err;

        while ( !peek( token::paren, ")" ) )
        {
            while ( peek( token::eol ) )
                fetch();

            if ( peek( token::paren, ")" ) )
                break;

            auto op = fetch();
            if ( op.cat != token::ident )
                return error( op, "expected function name" );

            if ( err = require( token::punct, "=" ) )
                return err;

            auto op_id = prog.get( op.data ).index;

            if ( peek( token::lambda ) )
            {
                fetch();
                function_t fn;
                if ( err = read_function( fn ) )
                    return err;
                out.functions[ op_id ] = std::move( fn );
            }
            else
            {
                auto target = fetch();
                if ( target.cat != token::ident )
                    return error( target, "expected builtin operation or lambda" );

                out.builtin_ops[ op_id ] = prog.get( target.data ).index;

                if ( err = require( token::eol ) )
                    return err;
            }
        }

        return nullptr;
    }

    diag reader::read_structure()
    {
        structure_t *out;
        diag err;

        ( err = read_name( out, prog.structures ) ) ||
        ( peek( token::punct, ":" ) && ( err = read_struct_sigs( *out ) ) ) ||
        ( err = require( token::eol ) ) ||
        ( err = require( token::paren, "(" ) ) ||
        ( err = read_struct_defs( *out ) ) ||
        ( err = require( token::paren, ")" ) );

        return err;
    }

}
