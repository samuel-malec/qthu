#pragma once

#include <iostream>
#include <sstream>
#include <string>
#include <variant>

#include "../frontend/ast.hpp"
#include "../../cthu_core/types.hpp"
#include "../ir/hir.hpp"
#include "../sema/analysis.hpp"

namespace qthu::jsc::print
{

struct pretty_printer
{
    std::string indent( int depth ) { return std::string( depth * 2, ' ' ); }

    void pad( std::ostream& out, int depth ) { out << indent( depth ); }
 
    void print_ast_expr( std::ostream& out, ast::expr& e, int depth );

    void print_ast_stmt( std::ostream& out, ast::stmt& s, int depth );

    void print_ast( std::ostream& out, ast::program& ast );

    void print_hir_expr( std::ostream& out, hir::function& fn, hir::expr_id e, int depth );

    void print_hir_stmt( std::ostream& out, hir::function& fn, hir::stmt_id s, int depth );

    void print_hir_function( std::ostream& out, hir::function& fn, sema::analysis_result& semantics );

    void print_hir( std::ostream& out, hir::module& mod, sema::analysis_result& semantics );

    void print_cthu( std::ostream& out, const qthu::cthuc::module_t& mod );
};

}
