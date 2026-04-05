#pragma once

#include "stack.hpp"
#include "dict.hpp"

#include <vector>
#include <map>

/* The state of a cthu program is rooted in set of stacks, though the values on
 * these stacks may be references to other objects (this particular
 * implementation has off-stack dictionaries and sequences, for instance).
 * However, any and all operands come from these stacks, and any off-stack
 * effects must be, ultimately, mediated by type-specific physical instructions
 * (e.g. the aforementioned dictionaries).
 *
 * Importantly, the physical layout of the stack is only relevant to the
 * implementation of physical opcodes – the language is structured in such a
 * way that stack layout of an abstraction layer cannot be observed by any
 * higher levels. For instance, the stack may be built from 32b words, but this
 * will be invisible to the program even if there are physical operations that
 * work with 64b words.
 *
 * Likewise, 64b integers may be implemented as a structure out of 32b physical
 * operations, without using any 64b-wide physical operations. Such a structure
 * will be indistinguishable from one built out of 64b physical operations
 * (except for execution speed). However, for the rest of the program, it will
 * be impossible to tell that the 64b values are internally represented as a
 * pair of 32b values.
 *
 * All of this is enforced by the type system, and by the fact that a value may
 * only ever be directly observed by an operation that belongs to its own
 * abstraction layer (i.e. it is part of some structure attached to its type). */

namespace cthu
{
    struct state
    {
        std::vector< stack_ptr > stacks;
        std::map< uint32_t, dict_ptr > objects;

        state snapshot() /* TODO make this more efficient later */
        {
            state snap = *this;

            for ( auto &sp : stacks )
                sp = sp->checkpoint();
            for ( auto &[ key, val ] : objects )
                val = val->checkpoint();

            return snap;
        }

        void push( int stack_id, word w )
        {
            auto &s = stacks[ stack_id ];
            s = s->push( w );
        }

        word pop( int stack_id )
        {
            return stacks[ stack_id ]->pop();
        }
    };
}
