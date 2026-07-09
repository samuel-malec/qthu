#pragma once

#include <cstdint>
#include <set>
#include <string>

namespace qthu::as
{

struct directive
{
    std::string mnemonic;
    std::string fn_name;
    uint32_t value;

    static const std::set< std::string_view > valid_directives;

    static directive from_string( std::string_view );

    bool has_fn_name() const { return !fn_name.empty(); }



};

}
