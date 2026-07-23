#pragma once
 
#include <cstdint>
#include <optional>
#include <variant>
#include <vector>
 
#include "../sema/types.hpp"
#include "../sema/analysis.hpp"
 
namespace qthu::jsc::hir
{
 
struct expr_id { std::uint32_t id; };
struct stmt_id { std::uint32_t id; };
 
struct expr
{
    struct int_lit    { std::uint64_t value; };
    struct bool_lit   { bool value; };
    struct var        { sema::binding_id id; };
    struct unary      { op_kind op; expr_id sub; };
    struct binary     { op_kind op; expr_id left; expr_id right; };
    struct assign     { sema::binding_id target; expr_id value; };
    struct call       { sema::function_id target; std::vector< expr_id > args; };
 
    type typ;
    std::variant<
        int_lit,
        bool_lit,
        var,
        unary,
        binary,
        assign,
        call
    > data;
};
 
struct stmt
{
    struct expr_stmt  { expr_id expr; };
    struct block      { std::vector< stmt_id > stmts; };
    struct let_stmt   { type typ; sema::binding_id target; std::optional< expr_id > value; };
    struct if_stmt    { expr_id cond; stmt_id then_branch; std::optional< stmt_id > else_branch; };
    struct loop_stmt  { stmt_id body; };
    struct ret_stmt   { std::optional< expr_id > value; };
    struct brk        {};
    struct cont       {};
 
    std::variant<
        expr_stmt,
        block,
        let_stmt,
        if_stmt,
        loop_stmt,
        ret_stmt,
        brk,
        cont
    > data;
};
 
struct function
{
    sema::function_id id;
    std::optional< sema::function_id > lexical_parent;
 
    std::vector< sema::binding_id > parameters;
    std::vector< sema::binding_id > captures;
 
    std::vector< expr > expressions;
    std::vector< stmt > statements;
    stmt_id body_root;
 
    const expr& get( expr_id eid ) const { return expressions[ eid.id ]; }
    const stmt& get( stmt_id sid ) const { return statements[ sid.id ]; }
};
 
struct module
{
    function script;
    std::vector< function > functions;
};
 
}
 