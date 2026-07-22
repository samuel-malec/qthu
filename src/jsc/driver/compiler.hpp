#pragma once

#include "config.hpp"
#include "../printer/pretty_printer.hpp"
#include "../../common/file.hpp"
#include "../frontend/token.hpp"
#include "../frontend/parser.hpp"
#include "../ir/hir.hpp"
#include "../lower/ast2hir.hpp"
#include "../sema/analysis.hpp"

namespace qthu::jsc
{

struct compiler
{
    void run( const config& conf )
    {
        std::string in_name = conf.in_name;
        std::string out_name = conf.out_name;

        source_ptr doc = std::make_shared< source_file >( in_name, read_file( in_name ) );

        print::pretty_printer printer{};
        parser p{ doc };
        auto ast = p.parse();
        if ( conf.emit_ast )
            printer.print_ast( std::cout, ast );

        sema::analyzer analyzer;
        auto semantics = analyzer.run( ast );
        hir::program hir_prog = hir::lower2hir( ast, semantics );
        if ( conf.emit_hir )
            printer.print_hir( std::cout, hir_prog, semantics );
    }
};

}
