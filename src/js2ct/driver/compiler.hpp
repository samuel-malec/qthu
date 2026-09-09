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
        std::string in_name = conf.file_in;
        std::string out_name = conf.file_out;

        source_ptr doc = std::make_shared< source_file >( in_name, read_file( in_name ) );

        print::pretty_printer printer{};
        parser p{ doc };
        auto ast = p.parse();
        if ( conf.emit_ast )
            printer.print_ast( std::cout, ast );

        sema::analyzer analyzer;
        auto semantics = analyzer.run( ast );

        hir::lowerer hir_lowerer{ semantics };
        hir::module hir = hir_lowerer.lower( ast );
        if ( conf.emit_hir )
            printer.print_hir( std::cout, hir, semantics );
        
        lin::lowerer lin_lowerer{ semantics };
        lin::program linear = lin_lowerer.lower( hir );
        if ( conf.emit_lin )
            printer.print_lin_program( std::cout, linear );

        cthu::lowerer cthu_lowerer{ semantics };
        cthu::module ct = cthu_lowerer.lower( linear );
        std::ofstream out( conf.file_out );
        if ( !out.is_open() )
            throw std::runtime_error( "Couldn't open file at: " + conf.file_out );
        
        printer.print_cthu( out, ct );
    }
};

}
