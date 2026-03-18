#pragma once

#include "lexer_base.hpp"

namespace qthu
{

// todo: this should fix the redundancy in instruction.cpp and directive.cpp
struct arg_parser : lexer_base
{
};

struct line_parser : lexer_base
{
    using lexer_base::lexer_base;
    
    void trim_comment()
    {
        auto ix = sv.find_first_of( ";#" );
        if ( ix != sv.npos )
            sv.remove_suffix( sv.length() - ix );
    }
};

} // namespace qthu