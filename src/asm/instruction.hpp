#pragma once

#include <cstdint>
#include <string>
#include <map>
#include <vector>
#include <stdexcept>

namespace qthu
{

using symtab_t = std::map< std::string, uint32_t >;

struct opr
{
    uint32_t raw = 0;

    opr() = default;
    opr( uint32_t r ) : raw( r ) { };

    std::string to_string() const
    {
        return std::to_string( raw );
    }

    uint32_t as_unsigned() const
    {
        return raw;
    }

    int32_t as_signed() const
    {
        return raw;
    }
};

struct addr
{
    std::string sym;
    int32_t off = 0;

    addr() = default;
    addr( int a ) : off( a ) { };
    addr( const char* s ) : sym( s ) { };
    addr( std::string_view s ) : sym( s ) { };
    addr( std::string s ) : sym( s ) { };

    addr( std::string_view s, int32_t o ) : sym( s ), off( o ) { }

    bool resolved() const
    {
        return sym.empty();
    }

    bool resolve( const symtab_t& symtab )
    {
        if ( resolved() )
            return true;

        auto it = symtab.find( sym );
        if ( it == symtab.end() )
            return false;

        off += it->second;
        sym.clear();
        return true;
    }

    int32_t value() const
    {
        return off;
    }
};

struct instruction
{
    std::string mnemonic;
    uint8_t opcode;
    uint8_t size;
    opr operand;
    addr address;
    bool has_address = false;

    static instruction from_string( const std::string_view );

    bool resolve( const symtab_t& symtab, uint32_t current_pc )
    {
        if ( !has_address )
            return true;

        if ( address.resolved() )
            return true;

        auto it = symtab.find( address.sym );
        if ( it == symtab.end() )
            return false;

        int32_t target = it->second;
        address.off = target - ( current_pc + 1 );
        address.sym.clear();

        return true;
    }

    template< typename T >
    inline void encode_int_le( std::vector< uint8_t >& buf, T value, uint8_t width )
    {
        auto u = static_cast< std::make_unsigned_t< T > >( value );
        for ( int i = 0; i < width; i++ )
            buf.push_back( static_cast< uint8_t >( ( u >> ( i * 8 ) ) & 0xFF ) );
    }

    std::vector< uint8_t > to_bytes() const
    {
        std::vector< uint8_t > bytes;
        bytes.push_back( opcode );
        if ( has_address && !address.resolved() )
            throw std::runtime_error( "Unresolved label operand during bytecode encoding" );

        uint32_t value = has_address ? static_cast< uint32_t >( address.value() ) : operand.raw;
        for ( int i = 0; i < size - 1; i++ )
            bytes.push_back( static_cast< uint8_t >( ( value >> ( i * 8 ) ) & 0xFF ) );

        return bytes;
    }
};

} // namespace qthu
