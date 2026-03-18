#pragma once

#include <cstdint>
#include <limits>

namespace qthu
{

inline bool within_i32( int64_t v )
{
    return v >= std::numeric_limits< int >::min() && v <= std::numeric_limits< int >::max();
}

} // namespace qthu