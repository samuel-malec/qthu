#pragma once
#include "stack.hpp"
#include <map>

/* Limited to scalar types, the basic cthu language is not very expressive. To
 * support more complex programs and especially more complex data, cthu
 * provides two composite types – dictionaries and sequences (though the latter
 * is really just an optimization of the former). However, regular dictionaries
 * (composed entirely of values) would be unable to support the powerful value
 * abstractions provided in the core language.
 *
 * Instead of a key → value map, dictionaries in cthu provide a collection of
 * stacks, organized such that the top element of each stack is considered a
 * searchable key. To make this work, the dictionary must be provided by a
 * structure which implements the ‹hash[ key_type ]› signature instance.
 *
 * This allows all core concepts to transfer over from the regular ‘top-level’
 * stacks to dictionaries. In particular, any composite values that are
 * conceived through user-defined structures will transfer over to
 * dictionaries. */

namespace cthu
{
    struct dict : brq::refcount_base<>, std::map< uint32_t, stack_ptr >
    {
        using ptr = brq::refcount_ptr< dict >;
        using std::map< uint32_t, stack_ptr >::map;

        ptr checkpoint()
        {
            return ptr( new dict( *this ) );
        }
    };

    using dict_ptr  = brq::refcount_ptr< dict >;
}
