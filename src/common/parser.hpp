#pragma once

#include "lexer.hpp"

namespace qthu
{

// todo: this should fix the redundancy in instruction.cpp and directive.cpp
struct arg_parser : lexer
{
};

struct line_parser : lexer
{
    using lexer::lexer;

    void trim_comment()
    {
        auto ix = sv.find_first_of( ";#" );
        if ( ix != sv.npos )
            sv.remove_suffix( sv.length() - ix );
    }
};

} // namespace qthu