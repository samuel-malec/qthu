#pragma once

#include <vector>
#include <string>
#include <map>

#include "lexer.hpp"

namespace qthu
{

using bytes = std::vector< uint8_t >;

struct FunctionMetadata
{
    std::string name = "main";
    uint16_t arg_count = 0;
    uint16_t var_count = 0;
    uint16_t stack_size = 4;
    bool strict_mode = true;
};

struct CompiledFunction
{
    std::string name;
    FunctionMetadata meta;
    std::vector< uint8_t > bytecode;
};

FunctionMetadata parse_metadata( const std::vector< Token >& tokens, size_t& idx );

bool no_operands( std::string format );

bytes assemble_function( const std::vector< Token >& tokens, size_t& token_idx,
                         const FunctionMetadata& meta,
                         const std::map< std::string, OPCodeInfo >& opcode_map );

bytes wrap_function_bytecode( const bytes& bytecode_body, const FunctionMetadata& meta,
                              const std::vector< CompiledFunction >& cpool );

bytes assemble_file( const std::string& content );

} // namespace qthu
