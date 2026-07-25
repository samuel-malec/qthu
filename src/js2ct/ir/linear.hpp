#pragma once
 
#include <cstdint>
#include <optional>
#include <variant>
#include <vector>
 
#include "../sema/types.hpp"
 
/**
 * The purpose of this IR is to enforce linearity that Cthulhu requires
 * This is done by numbering each expression in the original code, such that the linearity condition is satisfied
 */
namespace qthu::js2ct::lin
{
 
struct value_id 
{ 
    uint32_t value;
    auto operator<=>( const value_id& ) const = default;
};
 
struct value
{
    value_id id;
    uint32_t version = 0;
};
 
inline bool operator<( const value& lhs, const value& rhs )
{
    return lhs.id.value != rhs.id.value ? lhs.id.value < rhs.id.value : lhs.version < rhs.version;
}
 
using constant = std::variant< uint64_t, bool >;
using argument = std::variant< constant, value >;
 
struct instr;
 
struct cons_data
{
    constant c;
    value target;
};

struct unary_data
{
    op_kind op;
    argument arg1;
    value target;
};
 
struct binary_data
{
    op_kind op;
    argument arg1;
    argument arg2;
    value target;
};
 
struct copy_data
{
    argument arg1;
    value target;
};
 
struct if_data
{
    argument cond;
    std::vector< instr > then_body;
    std::vector< instr > else_body;
    std::optional< value > result;
};
 
struct loop_data
{
    std::vector< instr > body;
};
 
struct call_data
{
    sema::function_id callee;
    std::vector< argument > args;
    value target;
};
 
struct ret_data
{
    std::optional< argument > arg;
};
 
struct brk_data {};
struct cont_data {};
 
struct instr
{
    using data_type = std::variant<
                            cons_data,
                            unary_data,
                            binary_data,
                            copy_data,
                            if_data,
                            loop_data,
                            call_data,
                            ret_data,
                            brk_data,
                            cont_data >;
    data_type data;
 
    void for_each_use( auto&& f )
    {
        auto visit_operand = [ & ]( argument& o )
        {
            if ( auto* v = std::get_if< value >( &o ) )
                f( *v );
        };
 
        std::visit( [ & ]( auto&& d )
        {
            using T = std::decay_t< decltype( d ) >;
            if constexpr ( std::is_same_v< T, unary_data > )
                visit_operand( d.arg1 );
            else if constexpr ( std::is_same_v< T, binary_data > )
            {
                visit_operand( d.arg1 );
                visit_operand( d.arg2 );
            }
            else if constexpr ( std::is_same_v< T, copy_data > )
                visit_operand( d.arg1 );
            else if constexpr ( std::is_same_v< T, if_data > )
                visit_operand( d.cond );
            else if constexpr ( std::is_same_v< T, call_data > )
                for ( auto& a : d.args )
                    visit_operand( a );
            else if constexpr ( std::is_same_v< T, ret_data > )
            {
                if ( d.arg )
                    visit_operand( *d.arg );
            }
        }, data );
    }
 
    void set_target( value new_target )
    {
        std::visit( [ & ]( auto&& d )
        {
            using T = std::decay_t< decltype( d ) >;
            if constexpr ( requires { d.target; } )
                d.target = new_target;
        }, data );
    }
 
    std::optional< value > get_target()
    {
        return std::visit( []( auto&& d ) -> std::optional< value >
        {
            using T = std::decay_t< decltype( d ) >;
            if constexpr ( requires { d.target; } )
                return d.target;
            else
                return std::nullopt;
        }, data );
    }
};
 
// TODO: parameters
struct function
{
    sema::function_id name;
    std::vector< instr > body;
};
 
struct program
{
    std::vector< function > functions;

    function& get_script() { assert( !functions.empty() ); return functions[ 0 ]; }
};
 
}
