#pragma once

#include <string>

namespace qthu
{

struct BcBuilder
{
    void emit_opcode();

    void emit_operand();

    void create_function();

    void create_label();
};

} // namespace qthu
