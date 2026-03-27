#pragma once

#include <format>
#include <vector>
#include <map>
#include <iostream>

#include "brick-ptr"
#include "brick-min"

#include "lexer.hpp"

namespace cthu
{

struct atom
{
    uint32_t index = 0;

    std::strong_ordering operator<=>( const atom& o ) const = default;
    bool operator==( const atom& o ) const = default;
};

struct type_t {};
using type_ptr = type_t *;

struct inout_t
{
    enum { in, out } direction;
    type_ptr type;
};

struct sig_def_t
{
    std::vector< atom > in, out;
};

struct signature_t
{
    std::vector< atom > args;
    std::vector< std::pair< atom, std::vector< atom > > > inherits;
    std::map < atom, sig_def_t > defs;
};

using signature_ptr = signature_t*;

struct insn_t
{
    atom structure, operation;
    std::vector< atom > args;
};

struct function_t
{
    std::vector< uint32_t > in;
    std::vector< uint32_t > out;
    std::vector< insn_t > body;
};

using function_ptr = function_t *;

struct sig_instance_t
{
    uint32_t signature = 0;
    std::vector< uint32_t > args;
};

struct structure_t
{
    std::vector< sig_instance_t > signatures;
    std::map< uint32_t, uint32_t > builtin_ops;
    std::map< uint32_t, function_t > functions;
};

using structure_ptr = structure_t *;

struct symtab
{
    std::map< std::string, uint32_t, std::less<> > map;

    std::map< atom, type_t > types;
    std::map< atom, signature_t > signatures;
    std::map< atom, structure_t > structures;

    type_ptr get_type( atom a ) { return &types[ a ]; }
    signature_ptr get_signature( atom a ) { return &signatures[ a ]; }
    structure_ptr get_structure( atom a ) { return &structures[ a ]; }

    atom get( std::string_view name )
    {
        atom rv;

        if ( auto it = map.find( name ); it != map.end() )
            rv.index = it->second;
        else
        {
            rv.index = map.size();
            map.emplace( name, rv.index );
        }
        return rv;
    }

    bool has_type( std::string_view name )
    {
        return types.contains( get( name ) );
    }

    bool has_structure( std::string_view name )
    {
        return structures.contains( get( name ) );
    }

    bool has_signature( std::string_view name )
    {
        return signatures.contains( get( name ) );
    }

    type_ptr get_type( std::string_view name )
    {
        return get_type( get( name ) );
    }

    signature_ptr get_signature( std::string_view name )
    {
        return get_signature( get( name ) );
    }

    structure_ptr get_structure( std::string_view name )
    {
        return get_structure( get( name ) );
    }
};

struct diag_base : brq::refcount_base<>
{
    virtual void print( brq::string_builder& b ) const = 0;
    virtual bool is_fatal() const { return false; }
    virtual ~diag_base() = default;
};

using diag = brq::refcount_ptr< diag_base >;

struct diag_list : diag_base
{
    std::vector< diag > errs;
    void print( brq::string_builder& b ) const override;
};

struct diag_msg : diag_base
{
    brq::string_builder text;
    location loc;

    diag_msg( location l, auto... args ) 
        : loc{ l }
    {
        ( ( text << args ), ... );
    }

    void print( brq::string_builder& b ) const
    {
        b << loc << ": " << text.data() << "\n";
    }
};

struct reader : token_sink
{
    token current;
    lexer lex;
    symtab& prog;

    reader( source_ptr input, symtab& p ) 
        : lex{ input, *this }, prog{ p }
    {}

    token peek();
    token fetch();
    diag require( token::cat_t, std::string_view = "" );
    diag skip( token::cat_t );
    bool peek( token::cat_t, std::string_view = "" );
   
    diag parse();
    diag error( token t, auto... args );

    void push( token t ) override
    {
        current = std::move( t );
    }

    auto get_atom()
    {
        return [&]( token t ) { return prog.get( t.data ); };
    }

    diag read_function( function_t & );
    diag read_name( auto *&out, auto& map );
    diag read_ident_list( auto& out, auto f, std::string_view delim );

    diag read_structure();
    diag read_struct_defs( structure_t & );
    diag read_struct_sigs( structure_t & );
    diag read_sig_inherit( signature_t & );
    diag read_sig_args( signature_t & );
    diag read_sig_type( sig_def_t & );
    diag read_sig_def( signature_t & );
    diag read_sig_defs( signature_t & );
    diag read_signature();
    diag read_type();
};

}
