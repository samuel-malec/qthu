#pragma once

#include <string_view>
#include <string>
#include <vector>

namespace toy
{

struct token
{
    std::string_view lit;
    enum class category
    {
        unknown = 0,
        keyword,
        punct,
        ident,
        number,
    } cat;

    int32_t value;

    bool is( std::string_view sym ) const
    {
        return lit == sym;
    }

    bool is( category c ) const
    {
        return cat == c;
    }

    const char* begin() const
    {
        return lit.data();
    }

    const char* end() const
    {
        return begin() + lit.size();
    }

    explicit operator bool() const
    {
        return cat != category::unknown;
    }
};

inline constexpr std::string_view to_string( const token::category& c )
{
    using cat = token::category;
    switch ( c )
    {
    case cat::punct:
        return "punctuator";
    case cat::number:
        return "number";
    case cat::keyword:
        return "keyword";
    case cat::ident:
        return "identifier";
    default:
        return "unknown token";
    }
}

} // namespace jsc
