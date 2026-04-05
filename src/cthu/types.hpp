#pragma once

#include <cstdint>
#include <map>
#include <vector>

namespace cthu
{

struct atom
{
    uint32_t index = 0;

    std::strong_ordering operator<=>( const atom& o ) const = default;
    bool operator==( const atom& o ) const = default;
};

struct type_t {};
using type_ptr = type_t *;

struct inout_t
{
    enum { in, out } direction;
    type_ptr type;
};

struct sig_def_t
{
    std::vector< atom > in, out;
};

struct signature_t
{
    std::vector< atom > args;
    std::vector< std::pair< atom, std::vector< atom > > > inherits;
    std::map < atom, sig_def_t > defs;
};

using signature_ptr = signature_t*;

struct insn_t
{
    atom structure;
    atom operation;
    std::vector< atom > in;
    std::vector< atom > out;
};

struct function_t
{
    std::vector< atom > in;
    std::vector< atom > out;
    std::vector< insn_t > body;
};

using function_ptr = function_t *;

struct sig_instance_t
{
    atom signature;
    std::vector< atom > args;
};

struct structure_t
{
    std::vector< sig_instance_t > signatures;
    std::map< atom, atom > builtin_ops;
    std::map< atom, function_t > functions;
};

using structure_ptr = structure_t *;

}
