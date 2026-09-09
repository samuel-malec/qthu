#include <iostream>

#include "codegen.hpp"

namespace qthu::ct2qjs
{
    // This is not the semantics of cthulhu join, it is done this way to simplify the codegen
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

    // todo: we should probably have a precomputed map for these builtins...
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
        if ( name == "qjs_val_dup" || name == "qjs_val_dup" )
        {
            get1( insn );
            builder.add_instr( qthu::as::dup_() );
            builder.add_instr( qthu::as::put_loc_( insn.slots_out[ 0 ] ) );
            builder.add_instr( qthu::as::put_loc_( insn.slots_out[ 1 ] ) );
            return;
        }

        if ( name == "qjs_val_drop" )
            return;

        if ( name == "qjs_val_move" )
        {
            get1( insn );
            builder.add_instr( qthu::as::put_loc_( insn.slots_out[ 0 ] ) );
            return;
        }

        // push :: stack(array) x value -> stack. A "stack" is just a
        // jsvalue array (same representation cons_arr/get/set already use);
        // arr[arr.length] = value is a normal JS array-growing write, so
        // put_array_el_() already extends the length -- no atom/.length
        // work needed here (that's only needed on the shrinking side, pop).
        if ( name == "qjs_val_push" )
        {
            builder.add_instr( qthu::as::get_loc_( insn.slots_in[ 0 ] ) );  // survivor, returned as the new stack
            builder.add_instr( qthu::as::get_loc_( insn.slots_in[ 0 ] ) );  // consumed by get_length_
            builder.add_instr( qthu::as::get_length_() );
            builder.add_instr( qthu::as::get_loc_( insn.slots_in[ 1 ] ) );
            builder.add_instr( qthu::as::put_array_el_() );
            builder.add_instr( qthu::as::get_loc_( insn.slots_in[ 0 ] ) );
            builder.add_instr( qthu::as::put_loc_( insn.slots_out[ 0 ] ) );
            return;
        }

        // pop :: stack(array) -> value x stack. Fetches arr[length-1], then
        // truncates via arr.length = length-1 -- writing JS_ATOM_length on
        // an array-class object routes through QuickJS's set_array_length()
        // (quickjs.c), which actually frees the now out-of-range element,
        // unlike a plain indexed write/delete would. slots_out order
        // matches prelude.ct's `pop :: S -> T x S` (value, then stack).
        //
        // Both put_loc_(slots_out[...]) are deferred to the very end, after
        // every get_loc_(slots_in[0]) read: aloc_slots() frees an
        // instruction's input slots before allocating its outputs, so
        // slots_out[0] can alias slots_in[0] -- writing it early would
        // silently overwrite the array reference underneath every
        // subsequent read of it in this same case.
        if ( name == "qjs_val_pop" )
        {
            builder.add_instr( qthu::as::get_loc_( insn.slots_in[ 0 ] ) );
            builder.add_instr( qthu::as::get_length_() );
            builder.add_instr( qthu::as::push_i32_( 1 ) );
            builder.add_instr( qthu::as::sub_() );
            builder.add_instr( qthu::as::get_loc_( insn.slots_in[ 0 ] ) );
            builder.add_instr( qthu::as::swap_() );
            builder.add_instr( qthu::as::get_array_el_() );
            // stack: [ value ]

            builder.add_instr( qthu::as::get_loc_( insn.slots_in[ 0 ] ) );
            builder.add_instr( qthu::as::get_length_() );
            builder.add_instr( qthu::as::push_i32_( 1 ) );
            builder.add_instr( qthu::as::sub_() );
            builder.add_instr( qthu::as::get_loc_( insn.slots_in[ 0 ] ) );
            builder.add_instr( qthu::as::swap_() );
            builder.add_instr( qthu::as::put_field_( static_cast< int32_t >( js_atom_length ) ) );
            // stack: [ value ]  (put_field consumed obj+value, pushed nothing)

            builder.add_instr( qthu::as::get_loc_( insn.slots_in[ 0 ] ) );
            // stack: [ value, array ]

            builder.add_instr( qthu::as::put_loc_( insn.slots_out[ 1 ] ) );
            builder.add_instr( qthu::as::put_loc_( insn.slots_out[ 0 ] ) );
            return;
        }

        if ( name == "qjs_val_join" )
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

        if ( name == "qjs_val_add" )
        {
            binary_insn( insn, qthu::as::add_() );
            return;
        }

        if ( name == "qjs_val_sub" )
        {
            binary_insn( insn, qthu::as::sub_() );
            return;
        }

        if ( name == "qjs_val_mul" )
        {
            binary_insn( insn, qthu::as::mul_() );
            return;
        }

        if ( name == "qjs_val_div" )
        {
            binary_insn( insn, qthu::as::div_() );
            return;
        }

        if ( name == "qjs_val_rem" )
        {
            binary_insn( insn, qthu::as::mod_() );
            return;
        }

        if ( name == "qjs_val_eq" )
        {
            binary_insn( insn, qthu::as::eq_() );
            return;
        }

        if ( name == "qjs_val_ne" )
        {
            binary_insn( insn, qthu::as::neq_() );
            return;
        }

        if ( name == "qjs_val_lt" )
        {
            binary_insn( insn, qthu::as::lt_() );
            return;
        }

        if ( name == "qjs_val_le" )
        {
            binary_insn( insn, qthu::as::lte_() );
            return;
        }

        if ( name == "qjs_val_ge" )
        {
            binary_insn( insn, qthu::as::gte_() );
            return;
        }

        if ( name == "qjs_val_gt" )
        {
            binary_insn( insn, qthu::as::gt_() );
            return;
        }

        if ( name == "qjs_val_ashl" )
        {
            get2( insn );
            builder.add_instr( qthu::as::shl_() );
            builder.add_instr( qthu::as::put_loc_( insn.slots_out[ 0 ] ) );
            return;
        }

        if ( name == "qjs_val_ashr" )
        {
            get2( insn );
            builder.add_instr( qthu::as::shr_() );
            builder.add_instr( qthu::as::put_loc_( insn.slots_out[ 0 ] ) );
            return;
        }

        if ( name == "qjs_val_cons_obj" )
        {
            builder.add_instr( qthu::as::object_() );
            builder.add_instr( qthu::as::put_loc_( insn.slots_out[ 0 ] ) );
            return;
        }

        if ( name == "qjs_val_cons_arr" )
        {
            builder.add_instr( qthu::as::array_from_( 0 ) );
            builder.add_instr( qthu::as::put_loc_( insn.slots_out[ 0 ] ) );
            return;
        }

        if ( name == "qjs_val_cons_str" )
        {
            if ( !insn.resolved.literal )
                throw std::runtime_error( "qjs_val_cons_str requires a string literal operand" );

            uint32_t atom_index = register_atom( *insn.resolved.literal );
            builder.add_instr( qthu::as::push_atom_value_( static_cast< int32_t >( atom_index ) ) );
            builder.add_instr( qthu::as::put_loc_( insn.slots_out[ 0 ] ) );
            return;
        }

        if ( name == "qjs_val_get" )
        {
            binary_insn( insn, qthu::as::get_array_el_() );
            return;
        }

        if ( name == "qjs_val_set" )
        {
            // set: (obj, key, value) -> obj. put_array_el is consuming (obj
            // key value -> nothing), and doesn't hand the object back --
            // JS's `obj[k] = v` evaluates to v, not obj. So this isn't a
            // single opcode: dup the object, feed one copy to the store,
            // and put_loc the surviving copy as the result.
            builder.add_instr( qthu::as::get_loc_( insn.slots_in[ 0 ] ) );
            builder.add_instr( qthu::as::dup_() );
            builder.add_instr( qthu::as::get_loc_( insn.slots_in[ 1 ] ) );
            builder.add_instr( qthu::as::get_loc_( insn.slots_in[ 2 ] ) );
            builder.add_instr( qthu::as::put_array_el_() );
            builder.add_instr( qthu::as::put_loc_( insn.slots_out[ 0 ] ) );
            return;
        }

        if ( name.starts_with( "qjs_val_cons_" ) )
        {
            size_t offset = 13; // length of 'qjs_val_cons_'
            auto suffix = name.substr( offset );

            if ( suffix == "true" )
            {
                builder.add_instr( qthu::as::push_true_() );
                builder.add_instr( qthu::as::put_loc_( insn.slots_out[ 0 ] ) );
                return;
            }

            if ( suffix == "false" )
            {
                builder.add_instr( qthu::as::push_false_() );
                builder.add_instr( qthu::as::put_loc_( insn.slots_out[ 0 ] ) );
                return;
            }

            int result{};
            auto [ ptr, ec ] = std::from_chars( name.data() + offset, name.data() + name.size(), result);

            if ( ec == std::errc::invalid_argument )
                throw std::runtime_error( std::string( name ) + std::string(" argument of cons_ is not a number" ) );
            else if ( ec == std::errc() )
            {
                builder.add_instr( qthu::as::push_i32_( result ) );
                builder.add_instr( qthu::as::put_loc_( insn.slots_out[ 0 ] ) );
            }

            return;
        }

        if ( name == "qjs_val_lnot" )
        {
            get1( insn );
            builder.add_instr( qthu::as::lnot_() );
            builder.add_instr( qthu::as::put_loc_( insn.slots_out[ 0 ] ) );
            return;
        }

        if ( name == "qjs_val_bnot" )
        {
            get1( insn );
            builder.add_instr( qthu::as::not_() );
            builder.add_instr( qthu::as::put_loc_( insn.slots_out[ 0 ] ) );
            return;
        }

        if ( name == "qjs_val_and" )
        {
            binary_insn( insn, qthu::as::and_() );
            return;
        }

        if ( name == "qjs_val_or" )
        {
            binary_insn( insn, qthu::as::or_() );
            return;
        }

        if ( name == "qjs_val_xor" )
        {
            binary_insn( insn, qthu::as::xor_() );
            return;
        }

        if ( name == "qjs_val_assert" )
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
