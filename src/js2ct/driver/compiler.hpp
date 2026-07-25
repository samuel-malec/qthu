#pragma once

#include "config.hpp"
#include "../printer/pretty_printer.hpp"
#include "../../common/file.hpp"
#include "../frontend/token.hpp"
#include "../frontend/parser.hpp"
#include "../ir/hir.hpp"
#include "../ir/cthu.hpp"
#include "../lower/ast2hir.hpp"
#include "../lower/hir2linear.hpp"
#include "../lower/linear2cthu.hpp"
#include "../sema/analysis.hpp"

namespace qthu::js2ct
{

struct compiler
{
    void run( const config& conf )
    {
        std::string in_name = conf.in_path;
        std::string out_name = conf.out_path;

        source_ptr doc = std::make_shared< source_file >( in_name, read_file( in_name ) );

        print::pretty_printer printer{};
        parser p{ doc };
        auto ast = p.parse();
        if ( conf.emit_ast )
            printer.print_ast( std::cout, ast );

        sema::analyzer analyzer;
        auto semantics = analyzer.run( ast );

        hir::ast_lowerer alow{ semantics };
        hir::module hir = alow.lower_ast( ast );
        if ( conf.emit_hir )
            printer.print_hir( std::cout, hir, semantics );

        lin::program linear = lower_hir( hir, semantics );
        if ( conf.emit_lin )
            printer.print_lin_program( std::cout, linear );

        cthu::module ct = lower_linear( linear );
        std::ofstream out( conf.out_path );
        if ( !out.is_open() )
            throw std::runtime_error( "Couldn't open file at: " + conf.out_path );
        
        printer.print_cthu( out, ct );
    }
};

}
