#include <brick-bitlevel>
#include <brick-unit>

using namespace ::brick::bitlevel;

struct BitTupleTest
{
    using U10 = BitField< unsigned, 10 >;
    using T10_10 = BitTuple< U10, U10 >;

    int bitcount( uint32_t word ) {
        int i = 0;
        while ( word ) {
            if ( word & 1 )
                ++i;
            word >>= 1;
        }
        return i;
    }

    TEST(mask) {
        /* only works on little endian machines ... */
        ASSERT_EQ( 0xFF00u, bitlevel::mask( 8, 8 ) );
        ASSERT_EQ( 0xF000u, bitlevel::mask( 12, 4 ) );
        ASSERT_EQ( 0x0F00u, bitlevel::mask( 8, 4 ) );
        ASSERT_EQ( 60u, bitlevel::mask( 2, 4 ) );// 0b111100
        ASSERT_EQ( 28u, bitlevel::mask( 2, 3 ) );// 0b11100
    }

    TEST(bitcopy) {
        uint32_t a = 42, b = 11;
        bitlevel::bitcopy( BitPointer( &a ), BitPointer( &b ), 32 );
        ASSERT_EQ( a, b );
        a = 0xFF00;
        bitlevel::bitcopy( BitPointer( &a ), BitPointer( &b, 8 ), 24 );
        ASSERT_EQ( b, 0xFF0000u | 42u );
        a = 0;
        bitlevel::bitcopy( BitPointer( &b, 8 ), BitPointer( &a ), 24 );
        ASSERT_EQ( a, 0xFF00u );
        bitlevel::bitcopy( BitPointer( &a, 8 ), BitPointer( &b, 8 ), 8 );

        a = 0x3FF;
        b = 0;
        bitlevel::bitcopy( BitPointer( &a, 0 ), BitPointer( &b, 0 ), 10 );
        ASSERT_EQ( b, 0x3FFu );

        unsigned char from[32], to[32];
        std::memset( from, 0, 32 );
        std::memset( to, 0, 32 );
        from[0] = 1 << 7;
        bitlevel::bitcopy( BitPointer( from, 7 ), BitPointer( to, 7 ), 1 );
        ASSERT_EQ( int( to[0] ), int( from[ 0 ] ) );
        from[0] = 1;
        to[0] = 0;
        bitlevel::bitcopy( BitPointer( from, 0 ), BitPointer( to, 7 ), 1 );
        ASSERT_EQ( int( to[0] ), 1 << 7 );

        from[0] = 13;
        from[1] = 63;
        bitlevel::bitcopy( BitPointer( from, 0 ), BitPointer( to, 32 ), 16 );
        ASSERT_EQ( int( to[4] ), int( from[0] ) );
        ASSERT_EQ( int( to[5] ), int( from[1] ) );

        from[0] = 2;
        from[1] = 2;
        std::memset( to, 0, 32 );
        bitlevel::bitcopy( BitPointer( from, 1 ), BitPointer( to, 32 ), 16 );
        ASSERT_EQ( int( to[4] ), 1 );
        ASSERT_EQ( int( to[5] ), 1 );

        from[0] = 1;
        from[1] = 1;
        std::memset( to, 0, 32 );
        bitlevel::bitcopy( BitPointer( from, 0 ), BitPointer( to, 33 ), 16 );
        ASSERT_EQ( int( to[4] ), 2 );
        ASSERT_EQ( int( to[5] ), 2 );

        from[0] = 1;
        from[1] = 1;
        std::memset( to, 0, 32 );
        for ( int i = 0; i < 16; ++i )
            bitlevel::bitcopy( BitPointer( from, i ), BitPointer( to, 33 + i ), 1 );
        ASSERT_EQ( int( to[4] ), 2 );
        ASSERT_EQ( int( to[5] ), 2 );

        for ( int i = 0; i < 16; ++i )
            from[i] = 2;
        std::memset( to, 0, 32 );
        bitlevel::bitcopy( BitPointer( from, 1 ), BitPointer( to, 3 ), 128 );
        for ( int i = 0; i < 16; ++i )
            ASSERT_EQ( int( to[i] ), 8 );
    }

    TEST(field) {
        int a = 42, b = 0;
        typedef BitField< int, 10 > F;
        F::Virtual f;
        f.fromReference( BitPointer( &b ) );
        f.set( a );
        ASSERT_EQ( a, 42 );
        ASSERT_EQ( a, f );
    }

    TEST(basic) {
        T10_10 x;
        ASSERT_EQ( T10_10::bitwidth, 20 );
        ASSERT_EQ( T10_10::offset< 0 >(), 0 );
        ASSERT_EQ( T10_10::offset< 1 >(), 10 );
        auto a = get< 0 >( x );
        auto b = get< 1 >( x );
        a.set( 5 );
        b.set( 7 );
        ASSERT_EQ( a, 5u );
        ASSERT_EQ( b, 7u );
    }

    TEST(big) {
        bitlevel::BitTuple< BitField< uint64_t, 63 >, BitField< uint64_t, 63 > > x;
        ASSERT_EQ( x.bitwidth, 126 );
        ASSERT_EQ( x.offset< 0 >(), 0 );
        ASSERT_EQ( x.offset< 1 >(), 63 );
        get< 0 >( x ).set( (1ull << 62) + 7 );
        ASSERT_EQ( get< 0 >( x ), (1ull << 62) + 7 );
        ASSERT_EQ( get< 1 >( x ), 0u );
        get< 0 >( x ).set( 0 );
        get< 1 >( x ).set( (1ull << 62) + 7 );
        ASSERT_EQ( get< 0 >( x ), 0u );
        ASSERT_EQ( get< 1 >( x ), (1ull << 62) + 7 );
        get< 0 >( x ).set( (1ull << 62) + 11 );
        ASSERT_EQ( get< 0 >( x ), (1ull << 62) + 11 );
        ASSERT_EQ( get< 1 >( x ), (1ull << 62) + 7 );
    }

    TEST(structure) {
        bitlevel::BitTuple< BitField< std::pair< uint64_t, uint64_t >, 120 >, BitField< uint64_t, 63 > > x;
        auto v = std::make_pair( (uint64_t( 1 ) << 62) + 7, uint64_t( 33 ) );
        ASSERT_EQ( x.bitwidth, 183 );
        ASSERT_EQ( x.offset< 0 >(), 0 );
        ASSERT_EQ( x.offset< 1 >(), 120 );
        get< 1 >( x ).set( 333 );
        ASSERT_EQ( get< 1 >( x ), 333u );
        get< 0 >( x ).set( v );
        ASSERT_EQ( get< 1 >( x ), 333u );
        ASSERT( get< 0 >( x ).get() == v );
    }

    TEST(nested) {
        typedef bitlevel::BitTuple< T10_10, T10_10, BitField< unsigned, 3 > > X;
        X x;
        ASSERT_EQ( X::bitwidth, 43 );
        ASSERT_EQ( X::offset< 0 >(), 0 );
        ASSERT_EQ( X::offset< 1 >(), 20 );
        ASSERT_EQ( X::offset< 2 >(), 40 );
        auto a = get< 0 >( x );
        auto b = get< 1 >( x );
        get< 0 >( a ).set( 5 );
        get< 1 >( a ).set( 7 );
        get< 0 >( b ).set( 13 );
        get< 1 >( b ).set( 533 );
        get< 2 >( x ).set( 15 ); /* we expect to lose the MSB */
        ASSERT_EQ( get< 0 >( a ), 5u );
        ASSERT_EQ( get< 1 >( a ), 7u );
        ASSERT_EQ( get< 0 >( b ), 13u );
        ASSERT_EQ( get< 1 >( b ), 533u );
        ASSERT_EQ( get< 2 >( x ), 7u );
    }

    TEST(locked) {
        bitlevel::BitTuple<
            BitField< int, 15 >,
            BitLock,
            BitField< int, 16 >
        > bt;

        get< 1 >( bt ).lock();

        ASSERT_EQ( get< 0 >( bt ), 0 );
        ASSERT_EQ( get< 2 >( bt ), 0 );
        ASSERT( get< 1 >( bt ).locked() );
        ASSERT( get< 0 >( bt ).word() );

        get< 0 >( bt ) = 1;
        get< 2 >( bt ) = 1;

        ASSERT_EQ( get< 0 >( bt ), 1 );
        ASSERT_EQ( get< 2 >( bt ), 1 );

        ASSERT_EQ( bitcount( get< 0 >( bt ).word() ), 3 );

        get< 1 >( bt ).unlock();
        ASSERT_EQ( get< 0 >( bt ), 1 );
        ASSERT_EQ( get< 2 >( bt ), 1 );
        ASSERT( !get< 1 >( bt ).locked() );

        ASSERT_EQ( bitcount( get< 0 >( bt ).word() ), 2 );

        get< 0 >( bt ) = 0;
        get< 2 >( bt ) = 0;
        ASSERT( !get< 0 >( bt ).word() );
    }

    TEST(assign) /* TODO fails in DIVINE */
    {
        bitlevel::BitTuple<
            BitField< bool, 1 >,
            BitField< int, 6 >,
            BitField< bool, 1 >
        > tuple;

        get< 0 >( tuple ) = true;
        get< 2 >( tuple ) = get< 0 >( tuple );
        ASSERT( get< 2 >( tuple ).get() );
    }

    struct OperatorTester {
        int value;
        int expected;
        OperatorTester &operator++() { UNREACHABLE( "fell through" ); return *this; }
        OperatorTester operator++( int ) { UNREACHABLE( "fell through" ); return *this; }
        OperatorTester &operator--() { UNREACHABLE( "fell through" ); return *this; }
        OperatorTester &operator--( int ) { UNREACHABLE( "fell through" ); return *this; }
        OperatorTester &operator+=( int ) { UNREACHABLE( "fell through" ); return *this; }
        OperatorTester &operator-=( int ) { UNREACHABLE( "fell through" ); return *this; }
        OperatorTester &operator*=( int ) { UNREACHABLE( "fell through" ); return *this; }
        OperatorTester &operator/=( int ) { UNREACHABLE( "fell through" ); return *this; }
        OperatorTester &operator%=( int ) { UNREACHABLE( "fell through" ); return *this; }
        void test() { ASSERT_EQ( value, expected ); }
        void set( int v, int e ) { value = v; expected = e; }
    };
    struct TPrI : OperatorTester {
        TPrI &operator++() { ++value; return *this; }
    };
    struct TPoI : OperatorTester {
        TPoI operator++( int ) { auto r = *this; value++; return r; }
    };
    struct TPrD : OperatorTester {
        TPrD &operator--() { --value; return *this; }
    };
    struct TPoD : OperatorTester {
        TPoD operator--( int ) { auto r = *this; value--; return r; }
    };
    struct TPlO : OperatorTester {
        TPlO &operator+=( int v ) { value += v; return *this; }
    };
    struct TMO : OperatorTester {
        TMO &operator-=( int v ) { value -= v; return *this; }
    };
    struct TPoO : OperatorTester {
        TPoO &operator*=( int v ) { value *= v; return *this; }
    };
    struct TSO : OperatorTester {
        TSO &operator/=( int v ) { value /= v; return *this; }
    };
    struct TPrO : OperatorTester {
        TPrO &operator%=( int v ) { value %= v; return *this; }
    };

    template< int N, typename BT, typename L >
    void checkOperator( BT &bt, int v, int e, L l ) {
        auto t = get< N >( bt ).get();
        t.set( v, e );
        get< N >( bt ) = t;
        l( get< N >( bt ) );
        get< N >( bt ).get().test();
    }

#define CHECK( N, bt, v, e, test ) \
checkOperator< N >( bt, v, e, []( decltype( get< N >( bt ) ) item ) { test; } )

    TEST(operators) {
        bitlevel::BitTuple<
            BitField< bool, 4 >,
            BitField< TPrI >,// ++v
            BitField< TPoI >,// v++
            BitField< TPrD >,// --v
            BitField< TPoD >,// v--
            BitField< TPlO >,// v+=
            BitField< TMO >,// v-=
            BitField< TPoO >,// v*=
            BitField< TSO >,// v/=
            BitField< TPrO >,// v%=
            BitField< bool, 4 >
        > bt;

        CHECK( 1, bt, 0, 1, ++item );
        CHECK( 2, bt, 0, 1, item++ );
        CHECK( 3, bt, 0, -1, --item );
        CHECK( 4, bt, 0, -1, item-- );
        CHECK( 5, bt, 0, 5, item += 5 );
        CHECK( 6, bt, 0, -5, item -= 5 );
        CHECK( 7, bt, 2, 14, item *= 7 );
        CHECK( 8, bt, 42, 6, item /= 7 );
        CHECK( 9, bt, 42, 9, item %= 11 );
    }

#undef CHECK

    TEST(ones)
    {
        ASSERT_EQ( bitlevel::ones< uint32_t >( 0 ), 0 );
        ASSERT_EQ( bitlevel::ones< uint32_t >( 1 ), 1 );
        ASSERT_EQ( bitlevel::ones< uint32_t >( 2 ), 3 );
        ASSERT_EQ( bitlevel::ones< uint32_t >( 31 ), std::numeric_limits< uint32_t >::max() >> 1 );
        ASSERT_EQ( bitlevel::ones< uint32_t >( 32 ), std::numeric_limits< uint32_t >::max() );
        ASSERT_EQ( bitlevel::ones< uint32_t >( 33 ), std::numeric_limits< uint32_t >::max() );
    }
};

struct BitVecTest
{
    TEST(bvpair_shiftl)
    {
        using bvp32 = bitlevel::bvpair< uint16_t, uint16_t >;
        union {
            bvp32 bvp;
            uint32_t val;
        };
        bvp = bvp32( 23, 13 );
        uint32_t check = ( 13u << 16 ) | 23u;
        ASSERT_EQ( val, check );
        bvp = bvp << 7;
        check = check << 7;
        ASSERT_EQ( val, check );
        bvp = bvp << 18;
        check = check << 18;
        ASSERT_EQ( val, check );
        bvp = bvp32( 0xFF, 0xFF );
        check = (0xFF << 16) | 0xFF;
        bvp = bvp << 20;
        check = check << 20;
        ASSERT_EQ( val, check );
    }

    TEST(bvpair_shiftr)
    {
        using bvp32 = bitlevel::bvpair< uint16_t, uint16_t >;
        union {
            bvp32 bvp;
            uint32_t val;
        };
        bvp = bvp32( 23, 13 );
        uint32_t check = ( 13u << 16 ) | 23u;
        ASSERT_EQ( val, check );
        bvp = bvp >> 7;
        check = check >> 7;
        ASSERT_EQ( val, check );
        bvp = bvp >> 18;
        check = check >> 18;
        ASSERT_EQ( val, check );
        bvp = bvp32( 0xFF, 0xFF );
        check = (0xFF << 16) | 0xFF;
        bvp = bvp >> 20;
        check = check >> 20;
        ASSERT_EQ( val, check );
    }
};
