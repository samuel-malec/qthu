#include "codegen.hpp"
#include <iostream>

namespace qthu::cthucc
{
    void codegen::emit_fn_join( const lowered_insn& insn )
    {
        auto else_label = make_label();
        auto endif_label = make_label();
        auto check_label = make_label();

        builder.add_instr( qthu::as::get_loc_( insn.slots_in[ 0 ] ) );
        builder.add_instr( qthu::as::is_undefined_() );
        builder.add_instr( qthu::as::if_true_( else_label ) );
        builder.add_instr( qthu::as::get_loc_( insn.slots_in[ 0 ] ) );
        builder.add_instr( qthu::as::goto_( endif_label ) );
        
        builder.add_label( else_label );
        builder.add_instr( qthu::as::get_loc_( insn.slots_in[ 1 ] ) );
        builder.add_instr( qthu::as::is_undefined_() );
        builder.add_instr( qthu::as::if_false_( check_label ) );
        builder.add_instr( qthu::as::throw_() );

        builder.add_label( check_label );
        builder.add_instr( qthu::as::get_loc_( insn.slots_in[ 1 ] ) );
        
        builder.add_label( endif_label );
        builder.add_instr( qthu::as::put_loc_( insn.slots_out[ 0 ] ) );
    }

    void codegen::emit_fn_opt( const lowered_insn& insn )
    {
        auto then_label = make_label();
        auto end_label = make_label();

        builder.add_instr( qthu::as::get_loc_( insn.slots_in[ 0 ] ) );
        builder.add_instr( qthu::as::if_true_( then_label ) );

        builder.add_instr( qthu::as::undefined_() );
        builder.add_instr( qthu::as::put_loc_( insn.slots_out[ 0 ] ) );
        builder.add_instr( qthu::as::goto_( end_label ) );

        builder.add_label( then_label );
        builder.add_instr( qthu::as::get_loc_( insn.slots_in[ 1 ] ) );
        builder.add_instr( qthu::as::put_loc_( insn.slots_out[ 0 ] ) );

        builder.add_label( end_label );
    }

    void codegen::emit_builtin( const lowered_insn& insn, uint32_t uid )
    {
        auto get1 = [ & ]( const lowered_insn& insn )
        {
            builder.add_instr( qthu::as::get_loc_( insn.slots_in[ 0 ] ) );
        };

        auto get2 = [ & ]( const lowered_insn& insn )
        {
            builder.add_instr( qthu::as::get_loc_( insn.slots_in[ 0 ] ) );
            builder.add_instr( qthu::as::get_loc_( insn.slots_in[ 1 ] ) );
        };

        auto get_n = [ & ]( const lowered_insn& insn, int n )
        {
            for ( int i = 0; i < n; ++i )
                builder.add_instr( qthu::as::get_loc_( insn.slots_in[ i ] ) );
        };

        auto binary_insn = [ & ]( const lowered_insn& li, const qthu::as::instruction& asi )
        {
            get2( insn );
            builder.add_instr( asi );
            builder.add_instr( qthu::as::put_loc_( insn.slots_out[ 0 ] ) );
        };
        
        auto name = ir.st.name_of( insn.resolved.target );
        if ( name == "qjs_int_dup" || name == "qjs_bool_dup" )
        {
            get1( insn );
            builder.add_instr( qthu::as::dup_() );
            builder.add_instr( qthu::as::put_loc_( insn.slots_out[ 0 ] ) );
            builder.add_instr( qthu::as::put_loc_( insn.slots_out[ 1 ] ) );
            return;
        }

        if ( name == "qjs_int_drop" )
            return;

        if ( name == "qjs_int_move" )
        {
            get1( insn );
            builder.add_instr( qthu::as::put_loc_( insn.slots_out[ 0 ] ) );
            return;
        }

        if ( name == "qjs_int_push" )
        {
            std::cout << name << '\n';
            std::cout << "not implemented\n";
            return;
        }

        if ( name == "qjs_int_pop" )
        {
            std::cout << name << '\n';
            std::cout << "not implemented\n";
            return;
        }

        if ( name == "qjs_int_join" )
        {
            auto else_label = make_label();
            auto endif_label = make_label();

            builder.add_instr( qthu::as::get_loc_( insn.slots_in[ 0 ] ) );
            builder.add_instr( qthu::as::is_undefined_() );
            builder.add_instr( qthu::as::if_true_( else_label ) );
            builder.add_instr( qthu::as::get_loc_( insn.slots_in[ 0 ] ) );
            builder.add_instr( qthu::as::put_loc_( insn.slots_out[ 0 ] ) );
            builder.add_instr( qthu::as::goto_( endif_label ) );

            builder.add_label( else_label );
            builder.add_instr( qthu::as::get_loc_( insn.slots_in[ 1 ] ) );
            builder.add_instr( qthu::as::put_loc_( insn.slots_out[ 0 ] ) );
            builder.add_label( endif_label );
            return;
        }

        if ( name == "qjs_int_add" )
        {
            binary_insn( insn, qthu::as::add_() );
            return;
        }

        if ( name == "qjs_int_sub" )
        {
            binary_insn( insn, qthu::as::sub_() );
            return;
        }

        if ( name == "qjs_int_mul" )
        {
            binary_insn( insn, qthu::as::mul_() );
            return;
        }

        if ( name == "qjs_int_div" )
        {
            binary_insn( insn, qthu::as::div_() );
            return;
        }

        if ( name == "qjs_int_rem" )
        {
            binary_insn( insn, qthu::as::mod_() );
            return;
        }

        if ( name == "qjs_int_eq" )
        {
            binary_insn( insn, qthu::as::eq_() );
            return;
        }

        if ( name == "qjs_int_ne" )
        {
            binary_insn( insn, qthu::as::neq_() );
            return;
        }

        if ( name == "qjs_int_lt" )
        {
            binary_insn( insn, qthu::as::lt_() );
            return;
        }

        if ( name == "qjs_int_le" )
        {
            binary_insn( insn, qthu::as::lte_() );
            return;
        }

        if ( name == "qjs_int_ge" )
        {
            binary_insn( insn, qthu::as::gte_() );
            return;
        }

        if ( name == "qjs_int_gt" )
        {
            binary_insn( insn, qthu::as::gt_() );
            return;
        }

        if ( name == "qjs_int_ashl" )
        {
            get1( insn );
            builder.add_instr( qthu::as::shl_() );
            builder.add_instr( qthu::as::put_loc_( insn.slots_out[ 0 ] ) );
            return;
        }

        if ( name == "qjs_int_ashr" )
        {
            get1( insn );
            builder.add_instr( qthu::as::shr_() );
            builder.add_instr( qthu::as::put_loc_( insn.slots_out[ 0 ] ) );
            return;
        }

        if ( name.starts_with( "qjs_int_cons_" ) )
        {
            int result{};
            auto [ ptr, ec ] = std::from_chars( name.data() + 13, name.data() + name.size(), result);
            builder.add_instr( qthu::as::push_i32_( result ) );
            builder.add_instr( qthu::as::put_loc_( insn.slots_out[ 0 ] ) );

            if ( ec == std::errc::invalid_argument )
                throw std::runtime_error( std::string( name ) + std::string(" argument of cons_ is not a number" ) );
            else if ( ec == std::errc() )
            {
                builder.add_instr( qthu::as::push_i32_( result ) );
                builder.add_instr( qthu::as::put_loc_( insn.slots_out[ 0 ] ) );   
            }

            return;
        }

        if ( name == "qjs_bool_not" )
        {
            get1( insn );
            builder.add_instr( qthu::as::not_() );
            return;
    }

        if ( name == "qjs_bool_and" )
        {
            binary_insn( insn, qthu::as::and_() );
            return;
        }

        if ( name == "qjs_bool_or" )
        {
            binary_insn( insn, qthu::as::or_() );
            return;
        }

        if ( name == "qjs_bool_xor" )
        {
            binary_insn( insn, qthu::as::xor_() );
            return;
        }

        if ( name == "qjs_bool_assert" )
        {
            auto label = make_label();
            get1( insn );
            builder.add_instr( qthu::as::if_true_( label ) );
            builder.add_instr( qthu::as::throw_() );
            builder.add_label( label );
            return;
        }

        throw std::runtime_error( std::string( "unimplemented builtin: " ) + std::string( name ) );
    }
}
