#pragma once

#include <map>
#include <vector>
#include <string>

#include "types.hpp"
#include "symtab.hpp"

namespace qthu::cthucc
{

struct op
{
    enum cat
    {
        builtin,
        lambda,
    } c;

    std::vector< atom > in;
    std::vector< atom > out;
};

struct function
{
    atom name;
    std::vector< op > body;
};

struct program
{

};

program resolve( symtab& st )
{
    program prog{};

    return prog;
}

}
