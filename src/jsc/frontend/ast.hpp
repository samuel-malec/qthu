#pragma once

#include <cassert>
#include <cstdint>
#include <ostream>
#include <string>
#include <variant>
#include <optional>
#include <vector>

#include "token.hpp"
#include "../sema/types.hpp"

namespace jsc::ast {

struct expr;
struct stmt;
struct fn_decl;
struct var_decl;

struct expr
{
    enum cat_t 
    {
        num_lit,
        bool_lit,
        identifier,
        unary,
        binary,
        relational,
        assign,
        call,
    } cat;

    enum val_kind_t
    {
        lvalue,
        rvalue,
    } val_kind;

    location src_loc;
    std::variant< std::monostate, uint64_t, bool > val;
    std::string_view id;
    std::vector< expr > subs{};
    jsc::op_kind op;
    
    expr& operator[]( int n )
    {
        assert( n < subs.size() );
        return subs[ n ];
    }

    const expr& operator[]( int n ) const
    {
        assert( n < subs.size() );
        return subs[ n ];
    }
};

struct var_decl 
{
    enum mod_t { let, var, con } modifier;
    std::string_view name;
    std::optional< expr > e;
};

struct stmt
{
    enum cat_t 
    {
        ret,
        if_stmt,
        for_stmt,
        while_stmt,
        do_while_stmt,
        cont,
        brk,
        block,
        var_dclr,
        expr_stmt,
    } cat;

    location src_loc;
    std::vector< stmt > subs{};
    std::optional< expr > e;
    var_decl vdecl;

    stmt& operator[]( int n )
    {
        assert( n < subs.size() );
        return subs[ n ];
    }

    const stmt& operator[]( int n ) const
    {
        assert( n < subs.size() );
        return subs[ n ];
    }
};

struct fn_decl
{
    location src_loc;
    std::string_view name;
    std::vector< stmt > body;
    std::vector< var_decl > params;
};

using toplevel = std::variant< fn_decl, stmt >;

struct program 
{
    std::vector< toplevel > toplevel_items;
};

} // namespace dungeon
