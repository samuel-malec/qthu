#pragma once

#include <cstdint>
#include <set>
#include <string>

namespace qthu
{
struct directive
{
    std::string mnemonic;
    std::string name; // for function name
    uint32_t value;

    static const std::set< std::string_view > valid_directives;

    static directive from_string( const std::string_view );

    bool has_name() const
    {
        return !name.empty();
    }

    bool str_arg() const
    {
        return mnemonic == "function";
    }
};
} // namespace qthu
