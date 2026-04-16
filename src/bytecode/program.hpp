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

struct vardef
{
    uint32_t var_name_atom = 0; 
    int32_t scope_next = -1;
    uint16_t var_ref_idx = 0;
    uint8_t var_kind = 0;

    bool is_const = false;
    bool is_lexical = false;
    bool is_captured = false;
    bool has_scope = false;
};

struct closure_var
{
    uint32_t var_name_atom = 0;
    int32_t var_idx = 0;

    uint8_t closure_type = 0;
    bool is_const = false;
    bool is_lexical = false;
    uint8_t var_kind = 0;
};

struct function_bytecode
{
    std::string name;
    uint16_t arg_count = 0;
    uint16_t local_count = 0;
    uint16_t stack_size = 0;

    uint16_t flags = 0;
    uint8_t js_mode = 1;
    uint16_t defined_arg_count = 0;

    uint16_t var_ref_count = 0;
    std::vector< vardef > vardefs;
    std::vector< closure_var > closure_vars;

    std::vector< uint32_t > cpool_funcs; // indices into program.functions
    std::vector< uint8_t > bytecode;
    std::map< std::string, uint32_t > local_labels;
};

struct program
{
    std::vector< function_bytecode > functions;

    static uint8_t pack_vardef_flags( const vardef& vd )
    {
        uint8_t f = 0;
        f |= static_cast< uint8_t >( ( vd.var_kind & 0x0F ) << 0 );
        f |= static_cast< uint8_t >( ( vd.is_const ? 1 : 0 ) << 4 );
        f |= static_cast< uint8_t >( ( vd.is_lexical ? 1 : 0 ) << 5 );
        f |= static_cast< uint8_t >( ( vd.is_captured ? 1 : 0 ) << 6 );
        f |= static_cast< uint8_t >( ( vd.has_scope ? 1 : 0 ) << 7 );
        return f;
    }

    static uint16_t pack_closure_var_flags( const closure_var& cv )
    {
        uint16_t f = 0;
        f |= static_cast< uint16_t >( ( cv.closure_type & 0x07 ) << 0 );
        f |= static_cast< uint16_t >( ( cv.is_const ? 1 : 0 ) << 3 );
        f |= static_cast< uint16_t >( ( cv.is_lexical ? 1 : 0 ) << 4 );
        f |= static_cast< uint16_t >( ( cv.var_kind & 0x0F ) << 5 );
        return f;
    }

    void encode_function_object( bytes& result, uint32_t fn_index,
                                 std::vector< uint32_t >& active_stack ) const
    {
        if ( fn_index >= functions.size() )
            throw std::runtime_error( "invalid function index in constant pool" );

        for ( uint32_t a : active_stack )
            if ( a == fn_index )
                throw std::runtime_error( "cycle in function constant pools" );
        active_stack.push_back( fn_index );

        const auto& fn = functions[ fn_index ];
        const uint16_t flags = fn.flags ? fn.flags : default_flags();
        const uint8_t js_mode = fn.js_mode;
        const uint16_t defined_arg_count = fn.defined_arg_count ? fn.defined_arg_count : fn.arg_count;
        const uint16_t var_ref_count = fn.var_ref_count;
        const int32_t closure_var_count = static_cast< int32_t >( fn.closure_vars.size() );

        std::vector< uint32_t > cpool = fn.cpool_funcs;
        if ( fn_index == 0 && cpool.empty() && functions.size() > 1 )
        {
            cpool.reserve( functions.size() - 1 );
            for ( uint32_t i = 1; i < functions.size(); ++i )
                cpool.push_back( i );
        }
        const int32_t cpool_count = static_cast< int32_t >( cpool.size() );
        const int32_t byte_code_len = static_cast< int32_t >( fn.bytecode.size() );

        const uint32_t vardef_count = fn.vardefs.empty()
            ? static_cast< uint32_t >( fn.arg_count + fn.local_count )
            : static_cast< uint32_t >( fn.vardefs.size() );

        result.push_back( 0x0c ); // BC_TAG_FUNCTION_BYTECODE (0x0c)
        encode_u16( result, flags );
        encode_u8( result, js_mode );
        encode_atom( result, 0 );

        encode_leb128_u( result, fn.arg_count );
        encode_leb128_u( result, fn.local_count );
        encode_leb128_u( result, defined_arg_count );
        encode_leb128_u( result, fn.stack_size );
        encode_leb128_u( result, var_ref_count );
        encode_leb128_i( result, closure_var_count );
        encode_leb128_i( result, cpool_count );
        encode_leb128_i( result, byte_code_len );
        encode_leb128_i( result, static_cast< int32_t >( vardef_count ) );

        if ( vardef_count != 0 )
        {
            if ( !fn.vardefs.empty() && fn.vardefs.size() != vardef_count )
                throw std::runtime_error( "vardefs size mismatch" );

            for ( uint32_t i = 0; i < vardef_count; ++i )
            {
                vardef vd;
                if ( !fn.vardefs.empty() )
                    vd = fn.vardefs[i];

                encode_atom( result, vd.var_name_atom );
                encode_leb128_i( result, vd.scope_next + 1 );
                encode_leb128_u( result, vd.var_ref_idx );
                encode_u8( result, pack_vardef_flags( vd ) );
            }
        }

        for ( const auto& cv : fn.closure_vars )
        {
            encode_atom( result, cv.var_name_atom );
            encode_leb128_i( result, cv.var_idx );
            encode_u16( result, pack_closure_var_flags( cv ) );
        }

        result.insert( result.end(), fn.bytecode.begin(), fn.bytecode.end() );

        for ( uint32_t child : cpool )
            encode_function_object( result, child, active_stack );

        active_stack.pop_back();
    }

    std::vector< uint8_t > wrap_with_metadata() const
    {
        if ( functions.empty() )
            throw std::runtime_error( "Cannot wrap empty program" );

        if ( functions[ 0 ].name != "main" )
            throw std::runtime_error( "Entry function must be first and named 'main'" );

        bytes result;
        result.push_back( 0x05 ); // Bytecode format version
 
        std::vector< std::string > atoms;
        atoms.reserve( functions.size() );
        for ( const auto& func : functions )
            atoms.push_back( func.name );

        encode_leb128_u( result, static_cast< uint32_t >( atoms.size() ) );
        for ( const auto& atom : atoms )
            encode_string( result, atom );

        std::vector< uint32_t > active;
        encode_function_object( result, 0, active );
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

    static uint16_t default_flags()
    {
        return static_cast< uint16_t >( 1u << 1 );
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
            result += std::format( "  Bytecode size: {} bytes\n", func.bytecode.size() );

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
