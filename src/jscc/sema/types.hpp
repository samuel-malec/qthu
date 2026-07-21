#pragma once

namespace jscc
{

enum op_kind
{
    ADD, SUB, MUL, DIV,
    MOD, SHL, SHR,

    EQ, NEQ, LT, LEQ, GT, GEQ,

    NOT, AND, OR,
};

inline std::ostream& operator<<( std::ostream& os, const op_kind op )
{
    switch ( op )
    {
        case ADD:   return os << "+";
        case SUB:   return os << "-";
        case MUL:   return os << "*";
        case DIV:   return os << "/";
        case MOD:   return os << "%";
        case SHL:   return os << "<<";
        case SHR:   return os << ">>";
        case EQ:    return os << "==";
        case NEQ:   return os << "!=";
        case LT:    return os << "<";
        case LEQ:   return os << "<=";
        case GT:    return os << ">";
        case GEQ:   return os << ">=";
        case NOT:   return os << "!";
        case AND:   return os << "&&";
        case OR:    return os << "||";
    }

    return os << "idk";
}

enum class type
{
    jsvalue
};

}
