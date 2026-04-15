#pragma once

#include <vector>

#include "brick-ptr"
#include "brick-min"
#include "lexer.hpp"
#include "symtab.hpp"
#include "types.hpp"

namespace qthu::cthu
{

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
