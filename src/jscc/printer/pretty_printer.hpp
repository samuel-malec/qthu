#pragma once

#include <iostream>
#include <sstream>
#include <string>
#include <variant>

#include "../frontend/ast.hpp"

// TODO: Right now this is a utility printer for debugging purposes, later
// we should add a more sophisticated logging system, which would allow us to
// log information about compiler phases, such as duration of the phase,
// and specific metadata.

namespace jscc::print
{

struct pretty_printer
{
    using expr = ast::expr;
    using stmt = ast::stmt;
    using toplevel = ast::toplevel;
    using var_decl = ast::var_decl;
    using fn_decl = ast::fn_decl;

    std::string indent( int depth )
    {
        return std::string( depth * 2, ' ' );
    }

    void pad( int depth )
    {
        std::cout << indent( depth );
    }
 
    void print_expr( expr& e, int depth );

    void print_stmt( stmt& s, int depth );

    void print_ast( ast::program& ast );
};

}
