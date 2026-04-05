#pragma once
#include <compare>
#include <array>
#include <vector>

/* Signatures describe and group operations that can be performed on values.
 * Each operation is described by its name, informally given semantics, and a
 * formally given type.
 *
 * Operations consume and produce values, and the types of those values are
 * part of the signature. For instance, a binary arithmetic operation like
 * ‹add› may have a type ‹add ∷ T × T → T› meaning it consumes two values of
 * type ‹T› and produces one new value of the same type. An operation for
 * cloning values, called ‹dup›, may have a type ‹dup ∷ T → T × T› – it will
 * consume a single value and produce two new values of the same type.
 * Additionally, the informal semantics of ‹dup› demand that these new values
 * are both indistinguishable from the old value.
 *
 * The types which show up in a signature are always «type parameters» –
 * signatures cannot directly mention concrete types. Consider a signature
 * ‹arithmetic[ T ]› that covers the usual arithmetic operations and which has
 * a single type parameter, ‹T›. Then, the earlier example of an operation,
 * ‹add›, could be part of this signature. And if ‹w₃₂› is a «value type», then
 * a «signature instance» would be, for example, ‹arithmetic[ w₃₂ ]›.
 *
 * Both concrete types and concrete implementations (executable, formalised
 * semantics) for operations are given by a ‹▷structure›. A structure will be
 * said to «implement» a particular signature instance if it provides all its
 * operations with matching types. For instance the structure ‹bva₃₂› may
 * implement ‹arithmetic[ w₃₂ ]› by providing, among other things, an operation
 * ‹add ∷ w₃₂ × w₃₂ → w₃₂› which implements addition of 32-bit words.
 *
 * Definitions of a few basic signatures can be found in the ‹▷prelude›. */

namespace cthu
{
    struct type_t {};

    using type_ptr = type_t *;

    struct inout_t
    {
        enum { in, out } direction;
        type_ptr type;
    };

    struct op_t
    {
        std::array< inout_t, 4 > args;
    };
}
