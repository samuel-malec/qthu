#pragma once

#include <vector>
#include <cstdint>
#include <map>
#include <string>
#include <fstream>
#include <stdexcept>
#include <format>
#include "encoder.hpp"

namespace qthu::bc
{

using symtab_t = std::map< std::string, uint32_t >;

struct function_bytecode
{
    std::string name;
    uint16_t arg_count = 0;
    uint16_t local_count = 0;
    uint16_t stack_size = 0;

    std::vector< uint8_t > bytecode;
    symtab_t local_labels;

    uint64_t size() const
    {
        return bytecode.size();
    }
};

struct program
{
    std::vector< function_bytecode > functions;

    std::vector< uint8_t > wrap_with_metadata() const
    {
        if ( functions.empty() )
            throw std::runtime_error( "Cannot wrap empty program" );

        if ( functions[ 0 ].name != "main" )
            throw std::runtime_error( "Entry function must be first and named 'main'" );

        bytes result;
    
        // Bytecode format version (BC_VERSION)
        result.push_back( 0x05 );

        // Build atom table
        std::vector< std::string > atoms;
        for ( const auto& func : functions )
            atoms.push_back( func.name );

        encode_leb128_u( result, atoms.size() );
        for ( const auto& atom : atoms )
            encode_string( result, atom );

        const auto& main_func = functions[ 0 ];
        const auto& main_bytecode = main_func.bytecode;

        // BC_TAG_FUNCTION_BYTECODE (0x0c)
        result.push_back( 0x0c );

        // Function header
        uint16_t flags = 0x0001; // has simple parameter evaluation
        encode_u16( result, flags );
        encode_u8( result, 1 ); // jsMode

        encode_atom( result, 0 ); // function name atom index (entry function)
        encode_leb128_u( result, main_func.arg_count ); // arg_count
        encode_leb128_u( result, main_func.local_count ); // var_count
        encode_leb128_u( result, main_func.arg_count ); // defined_arg_count
        encode_leb128_u( result, main_func.stack_size ); // stack_size
        encode_leb128_u( result, 0 ); // var_ref_count
        encode_leb128_u( result, 0 ); // closure_var_count
        encode_leb128_u( result, static_cast< uint32_t >( functions.size() - 1 ) ); // cpool_count
        encode_leb128_u( result, static_cast< uint32_t >( main_bytecode.size() ) ); // byte_code_len
        encode_leb128_u( result, main_func.local_count + main_func.arg_count ); // vardef_count

        // Encode args
        for ( uint32_t i = 0; i < main_func.arg_count; i++ )
        {
            encode_atom( result, 0 ); // var_name
            encode_leb128_u( result, 0 ); // scope_next + 1 (0 means no next scope var)
            encode_leb128_u( result, 0 ); // var_ref_idx
            encode_u8( result, 0 ); // var_kind
        }

        // Encode locals
        for ( uint32_t i = 0; i < main_func.local_count; i++ )
        {
            encode_atom( result, 0 ); // var_name
            encode_leb128_u( result, 0 ); // scope_next + 1 (0 means no next scope var)
            encode_leb128_u( result, 0 ); // var_ref_idx
            encode_u8( result, 0 ); // var_kind
        }

        // Main function bytecode
        result.insert( result.end(), main_bytecode.begin(), main_bytecode.end() );

        // Nested functions (cpool)
        for ( std::size_t i = 1; i < functions.size(); ++i )
        {
            const auto& func = functions[ i ];
            const auto& func_bytecode = func.bytecode;

            result.push_back( 0x0c ); // BC_TAG_FUNCTION_BYTECODE
            uint16_t nested_flags = 0x0001;
            encode_u16( result, nested_flags );
            encode_u8( result, 1 ); // jsMode

            encode_atom( result, static_cast< uint32_t >( i ) ); // function name atom index
            encode_leb128_u( result, func.arg_count ); // arg_count
            encode_leb128_u( result, func.local_count ); // var_count
            encode_leb128_u( result, func.arg_count ); // defined_arg_count
            encode_leb128_u( result, func.stack_size ); // stack_size
            encode_leb128_u( result, 0 ); // var_ref_count
            encode_leb128_u( result, 0 ); // closure_var_count
            encode_leb128_u( result, 0 ); // cpool_count (no constants)
            encode_leb128_u( result,
                             static_cast< uint32_t >( func_bytecode.size() ) ); // byte_code_len
            encode_leb128_u( result, func.local_count + func.arg_count ); // vardef_count

            // Encode args
            for ( uint32_t j = 0; j < func.arg_count; j++ )
            {
                encode_atom( result, 0 );
                encode_leb128_u( result, 0 );
                encode_leb128_u( result, 0 );
                encode_u8( result, 0 );
            }

            // Encode locals
            for ( uint32_t j = 0; j < func.local_count; j++ )
            {
                encode_atom( result, 0 );
                encode_leb128_u( result, 0 );
                encode_leb128_u( result, 0 );
                encode_u8( result, 0 );
            }

            // Function bytecode
            result.insert( result.end(), func_bytecode.begin(), func_bytecode.end() );
        }

        return result;
    }

    std::vector< uint8_t > to_bytes() const
    {
        return wrap_with_metadata();
    }

    void write_binary( const std::string& filename ) const
    {
        auto bytes = to_bytes();
        std::ofstream out( filename, std::ios::binary );
        if ( !out )
            throw std::runtime_error( "failed to open file for writing: " + filename );
        out.write( reinterpret_cast< const char* >( bytes.data() ), bytes.size() );
    }

    std::string hex_dump() const
    {
        auto bytes = to_bytes();
        std::string result;
        char buf[ 64 ];
        for ( size_t i = 0; i < bytes.size(); ++i )
        {
            if ( i % 16 == 0 )
            {
                if ( i > 0 )
                    result += '\n';
                snprintf( buf, sizeof( buf ), "%08zx: ", i );
                result += buf;
            }
            snprintf( buf, sizeof( buf ), "%02x ", bytes[ i ] );
            result += buf;
        }
        result += '\n';
        return result;
    }

    std::string print() const
    {
        std::string result;
        for ( const auto& func : functions )
        {
            result += std::format( "Function: {} (args: {}, locals: {}, stack: {})\n", func.name,
                                   func.arg_count, func.local_count, func.stack_size );
            result += std::format( "  Bytecode size: {} bytes\n", func.size() );

            if ( !func.local_labels.empty() )
            {
                result += "  Labels:\n";
                for ( const auto& [ name, offset ] : func.local_labels )
                    result += std::format( "    {}: {}\n", name, offset );
            }

            result += "  Hex:\n    ";
            char buf[ 8 ];
            for ( size_t i = 0; i < func.bytecode.size(); ++i )
            {
                if ( i > 0 && i % 16 == 0 )
                    result += "\n    ";
                snprintf( buf, sizeof( buf ), "%02x ", func.bytecode[ i ] );
                result += buf;
            }
            result += "\n\n";
        }
        return result;
    }
};
} // namespace qthu
