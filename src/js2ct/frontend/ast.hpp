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

namespace qthu::js2ct::ast
{

struct expr;
struct stmt;

using expr_ptr = std::unique_ptr< expr >;
using stmt_ptr = std::unique_ptr< stmt >;

struct int_lit    
{
    std::uint64_t value; 
};

struct bool_lit
{
    bool value;
};

struct str_lit
{
    std::string_view value;
};

struct var        
{
    std::string_view name;
};

struct unary
{
    op_kind op;
    expr_ptr sub; 
};

struct binary
{
    op_kind op;
    expr_ptr left;
    expr_ptr right;
};

struct call
{
    expr_ptr callee;
    std::vector< expr_ptr > args;
};

// obj.x lowers with computed=false and key synthesized as a str_lit from
// the identifier text; arr[expr] lowers with computed=true and key as
// whatever expression was parsed -- computed is parse-time-only info
// (get is key-uniform, P3's design), not carried past ast2hir.
struct member
{
    expr_ptr object;
    expr_ptr key;
    bool computed;
};

// target is `var` for a plain `x = v`, or `member` for `obj.x = v` /
// `arr[i] = v` -- single-level only: a member target's own `object` must
// itself resolve to a `var` (checked in the parser), not another member
// expression (`a.b.c = v` is out of scope for now).
struct assign
{
    std::variant< var, member > target;
    expr_ptr value;
};

// Plain identifier or string-literal keys only; computed keys ({[expr]: v})
// are out of scope for now.
struct object_lit
{
    std::vector< std::pair< std::string_view, expr_ptr > > props;
};

struct array_lit
{
    std::vector< expr_ptr > elements;
};

struct expr
{
    location loc;

    std::variant<
        int_lit,
        bool_lit,
        str_lit,
        var,
        unary,
        binary,
        assign,
        call,
        object_lit,
        array_lit,
        member
    > data;
};

struct param 
{
    std::string_view name;
};

struct var_declarator 
{
    std::string_view name;
    std::optional< expr > init;
};

struct var_declaration 
{
    enum class kind_t { let, constant, var } kind;
    std::vector< var_declarator > declarators;
};

struct block 
{
    std::vector< stmt_ptr > stmts;
};

struct fn_declaration
{
    std::string_view name;
    std::vector< param > params;
    block body;
};

struct ret 
{
    std::optional< expr > value;
};

struct if_stmt
{
    expr cond;
    stmt_ptr then_branch;
    stmt_ptr else_branch;
};

struct do_while_stmt
{
    expr cond;
    stmt_ptr body;
};

struct while_stmt 
{
    expr cond;
    stmt_ptr body;
};

struct for_stmt
{
    stmt_ptr init;
    expr_ptr cond;
    expr_ptr update;
    stmt_ptr body;
};

struct expr_stmt 
{
    expr value;
};

struct brk{};

struct cont{};

struct stmt 
{
    location loc;

    std::variant<
        block,
        var_declaration,
        var_declarator,
        fn_declaration,
        ret,
        if_stmt,
        do_while_stmt,
        while_stmt,
        for_stmt,
        expr_stmt,
        brk,
        cont
    > data;
};

struct program 
{
    std::vector< stmt_ptr > statements;
};

}
