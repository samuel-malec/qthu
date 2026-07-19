#pragma once

#include <cassert>
#include <cstdint>
#include <ostream>
#include <string>
#include <variant>
#include <optional>
#include <vector>

#include "token.hpp"

namespace jscc::ast {

enum op_kind 
{
    ADD, SUB, MUL, DIV,
    MOD, SHL, SHR,

    EQ, NEQ, LT, LEQ, GT, GEQ,

    NOT, AND, OR,
};

inline std::ostream& operator<<( std::ostream& os, const op_kind op )
{
    switch ( op )
    {
        case ADD:   return os << "+";
        case SUB:   return os << "-";
        case MUL:   return os << "*";
        case DIV:   return os << "/";
        case MOD:   return os << "%";
        case SHL:   return os << "<<";
        case SHR:   return os << ">>";
        case EQ:    return os << "==";
        case NEQ:   return os << "!=";
        case LT:    return os << "<";
        case LEQ:   return os << "<=";
        case GT:    return os << ">";
        case GEQ:   return os << ">=";
        case NOT:   return os << "!";
        case AND:   return os << "&&";
        case OR:    return os << "||";
    }

    return os << "idk";
}

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

    location src_loc;
    std::variant< std::monostate, uint64_t, bool > val;
    std::string_view id;
    std::vector< expr > subs{};
    op_kind op;
    
    expr& operator[]( int n )
    {
        assert( n < subs.size() );
        return subs[ n ];
    }
};

struct var_decl 
{
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
};

struct fn_decl
{
    location src_loc;
    std::string_view name;
    std::vector< stmt > body;
    std::vector< var_decl > params;
};

using toplevel = std::variant< fn_decl, var_decl, stmt >;

struct program 
{
    std::vector< toplevel > toplevel_items;
};

} // namespace dungeon
