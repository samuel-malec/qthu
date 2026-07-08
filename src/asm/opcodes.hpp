#pragma once

#include <string>
#include <map>
#include <cstdint>
#include <unordered_set>
#include <optional>

namespace qthu::as
{

struct opcode_info
{
    std::uint8_t opcode;
    uint8_t size;
    std::string format;
};

inline std::map< std::string, opcode_info > build_opcode_map()
{
    std::map< std::string, opcode_info > opcode_map;
    uint8_t opcode_id = 0;

#define SHORT_OPCODES 1
#define DEF( id, size, n_pop, n_push, f ) opcode_map[ #id ] = opcode_info{ opcode_id++, size, #f };
#define def( id, size, n_pop, n_push, f )

#include "quickjs-opcode.h"

#undef SHORT_OPCODES
#undef DEF
#undef def

    return opcode_map;
}

inline std::unordered_set< std::string > build_opcode_set()
{
    auto map = build_opcode_map();
    std::unordered_set< std::string > opcodes;
    for ( const auto& [ name, info ] : map )
    {
        opcodes.insert( name );
    }
    return opcodes;
}

inline const std::map< std::string, opcode_info >& get_opcode_map()
{
    static auto map = build_opcode_map();
    return map;
}

inline const std::unordered_set< std::string >& get_opcode_set()
{
    static auto set = build_opcode_set();
    return set;
}

inline std::optional< opcode_info > get_opcode_info( const std::string& mnemonic )
{
    const auto& map = get_opcode_map();
    auto it = map.find( mnemonic );
    if ( it == map.end() )
        return std::nullopt;
    return it->second;
}

inline bool is_valid_instruction( const std::string& mnemonic )
{
    return get_opcode_set().count( mnemonic ) > 0;
}

}
