#pragma once
#include <brick-ptr>
#include <array>

namespace cthu
{
    using word = uint32_t;

    struct stack : brq::refcount_base<>
    {
        using ptr = brq::refcount_ptr< stack >;

        std::array< word, 14 > data;
        uint8_t count = 0;
        uint8_t p_count = 0;
        bool readonly = false;
        ptr parent;

        ptr checkpoint()
        {
            readonly = true;
            ptr rv( new stack );

            if ( count )
            {
                rv->parent.reset( this );
                rv->p_count = count;
            }
            else
            {
                rv->parent = parent;
                rv->p_count = parent->count;
            }

            return rv;
        }

        word pop()
        {
            ASSERT( !readonly );

            if ( count )
                return data[ --count ];

            word rv = parent->data[ --p_count ];

            if ( p_count == 0 )
            {
                parent = parent->parent;
                p_count = parent->count;
            }

            return rv;
        }

        ptr push( word w )
        {
            ASSERT( !readonly );

            if ( count < data.size() )
            {
                data[ count++ ] = w;
                return ptr( this );
            }

            readonly = true;
            ptr next( new stack );
            next->parent.reset( this );
            next->push( w );
            return next;
        }
    };

    using stack_ptr = brq::refcount_ptr< stack >;
}
