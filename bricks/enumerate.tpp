#include <iostream>
#include <iomanip>
#include "brick-unit"
#include "brick-nat"
#include "brick-enumerate"
#include <set>

auto enum_prefix( auto expect, auto do_enum )
{
    return [=]
    {
        decltype( expect ) enumerated;
        using elem_t = std::decay_t< decltype( *expect.begin() ) >;

        for ( int i = 0; i < expect.size(); ++i )
        {
            elem_t elem = do_enum( i );
            ASSERT( !enumerated.contains( elem ), i, elem );
            enumerated.insert( elem );
        }

        for ( auto i : enumerated )
            if ( !expect.contains( i ) )
                DEBUG( "extra: ", i );

        for ( auto i : expect )
            if ( !enumerated.contains( i ) )
                DEBUG( "missing: ", i );

        ASSERT_EQ( expect, enumerated );
    };
}

int main()
{
    using t2_t   = std::array< brq::nat, 2 >;
    using t3_t   = std::array< brq::nat, 3 >;
    using list_t = std::vector< brq::nat >;

    std::set< t2_t > t2_b_33 =
    {
        { 0, 0 },
        { 0, 1 }, { 1, 0 }, { 1, 1 },
        { 0, 2 }, { 1, 2 }, { 2, 0 }, { 2, 1 }, { 2, 2 }
    };

    std::set< t2_t > t2_b_26 =
    {
        { 0, 0 }, { 0, 1 }, { 1, 0 }, { 1, 1 },
        { 0, 2 }, { 1, 2 }, { 0, 3 }, { 1, 3 },
        { 0, 4 }, { 1, 4 }, { 0, 5 }, { 1, 5 }
    };

    std::set< t2_t > t2_b_62 =
    {
        { 0, 0 }, { 1, 0 }, { 0, 1 }, { 1, 1 },
        { 2, 0 }, { 2, 1 }, { 3, 0 }, { 3, 1 },
        { 4, 0 }, { 4, 1 }, { 5, 0 }, { 5, 1 }
    };

    std::set< t3_t > t3_b_223 =
    {
        { 0, 0, 0 }, { 0, 0, 1 }, { 0, 1, 0 }, { 0, 1, 1 },
        { 1, 0, 0 }, { 1, 0, 1 }, { 1, 1, 0 }, { 1, 1, 1 },
        { 0, 0, 2 }, { 0, 1, 2 }, { 1, 0, 2 }, { 1, 1, 2 }
    };

    std::set< list_t > lists =
    {
        {}, { 0 }, { 1 }, { 0, 0 }, { 0, 1 }, { 1, 0 }, { 1, 1 },
        { 2 }, { 0, 2 }, { 1, 2 }, { 2, 0 }, { 2, 1 }, { 2, 2 },
        { 3 }, { 0, 3 }, { 1, 3 }, { 2, 3 }, { 3, 0 }, { 3, 1 }, { 3, 2 }, { 3, 3 },

        { 0, 0, 0 }, { 0, 0, 1 }, { 0, 1, 0 }, { 0, 1, 1 },
        { 1, 0, 0 }, { 1, 1, 0 }, { 1, 0, 1 }, { 1, 1, 1 },

        { 0, 0, 2 }, { 1, 0, 2 }, { 0, 1, 2 }, { 1, 1, 2 },
        { 0, 2, 0 }, { 1, 2, 0 }, { 0, 2, 1 }, { 1, 2, 1 },
        { 0, 2, 2 }, { 1, 2, 2 }, { 2, 0, 0 }, { 2, 1, 0 },
        { 2, 2, 0 }, { 2, 0, 1 }, { 2, 1, 1 }, { 2, 2, 1 },
        { 2, 0, 2 }, { 2, 1, 2 }, { 2, 2, 2 }
    };

    std::set< list_t > sets_3 =
    {
        {}, { 0 }, { 1 }, { 0, 1 }, { 2 }, { 0, 2 }, { 1, 2 }, { 0, 1, 2 }
    };

    using namespace brq::enumerate;

    brq::test_case( "tuple 2" ) =
        enum_prefix( t2_b_33, []( auto i ) { return tuple< 2 >( i ); } );

    brq::test_case( "tuple 2 bounded (3, 3)" ) =
        enum_prefix( t2_b_33, []( auto i ) { return tuple< 2 >( i ); } );

    brq::test_case( "tuple 3 bounded (2, 2, 3)" ) =
        enum_prefix( t3_b_223, []( auto i ) { return tuple< 3 >( i, std::array{ 2, 2, 3 } ); } );

    brq::test_case( "tuple 2 bounded (2, 6)" ) =
        enum_prefix( t2_b_26, []( auto i ) { return tuple< 2 >( i, std::array{ 2, 6 } ); } );
    brq::test_case( "tuple 2 bounded (6, 2)" ) =
        enum_prefix( t2_b_62, []( auto i ) { return tuple< 2 >( i, std::array{ 6, 2 } ); } );

    brq::test_case( "list" ) =
        enum_prefix( lists, []( auto i ) { return list( i ); } );
    brq::test_case( "sets" ) =
        enum_prefix( sets_3, []( auto i ) { return set( i ); } );

    brq::test_case( "ints" ) =
        enum_prefix( std::set{ 0, -1, 1, -2, 2 }, []( auto i ) { return integer( i ); } );
}
