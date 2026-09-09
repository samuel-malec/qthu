#pragma once

#include <string>
#include <map>
#include <vector>

// We should eventually unify cthu representatations used by js2ct and ct2qjs, but
namespace qthu::js2ct::cthu
{

using name = std::string;

struct sig_def
{
    std::vector< name > in, out;
};

struct signature
{
    std::vector< name > args;
    std::vector< std::pair< name, std::vector< name > > > inherits;
    std::map < name, sig_def > defs;
};

using signature_ptr = signature*;

struct insn
{
    name structure;
    name operation;
    std::vector< name > in;
    std::vector< name > out;
};

struct function
{
    std::vector< name > in;
    std::vector< name > out;
    std::vector< insn > body;
};

using function_ptr = function *;

struct sig_instance
{
    name signature;
    std::vector< name > args;
};

struct structure
{
    name id;
    std::map< name, function > functions;
};

using structure_ptr = structure *;

struct module
{
    std::vector< structure > structures;
};

}
