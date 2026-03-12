#if 0
namespace t_brq
{
    struct test_spawn
    {
        TEST( basic_true )
        {
            auto r = brq::spawn_and_wait( "true" );
            ASSERT_EQ( r.exitcode(), 0 );
            ASSERT_EQ( r.signal(), 0 );
            ASSERT( r );
        }

        TEST( basic_false )
        {
            auto r = brq::spawn_and_wait( "false" );
            ASSERT_LT( 0, r.exitcode() );
            ASSERT_EQ( r.signal(), 0 );
            ASSERT( !r );
        }

        TEST( echo1 )
        {
            auto r = brq::spawn_and_wait( brq::capture_stdout, "printf", "a" );
            ASSERT( r );
            ASSERT_EQ( r.out(), "a" );
            ASSERT_EQ( r.err(), "" );
        }

        TEST( echo2 )
        {
            auto r = brq::spawn_and_wait( brq::capture_stdout | brq::capture_stderr, "printf", "a" );
            ASSERT( r );
            ASSERT_EQ( r.out(), "a" );
            ASSERT_EQ( r.err(), "" );
        }

        TEST( echo_spec )
        {
            auto r = brq::spawn_and_wait( brq::capture_stdout, "printf", "a\nb" );
            ASSERT( r );
            ASSERT_EQ( r.out(), "a\nb" );
            ASSERT_EQ( r.err(), "" );
        }

        TEST( shell_echo_stdout )
        {
            auto r = brq::shell_spawn_and_wait( brq::capture_stdout, "printf a" );
            ASSERT( r );
            ASSERT_EQ( r.out(), "a" );
            ASSERT_EQ( r.err(), "" );
        }

        TEST( shell_echo_stderr )
        {
            auto r = brq::shell_spawn_and_wait( brq::capture_stdout |
                                                brq::capture_stderr, "printf a >&2" );
            ASSERT( r );
            ASSERT_EQ( r.out(), "" );
            ASSERT_EQ( r.err(), "a" );
        }

        TEST( in_basic )
        {
            auto r = brq::spawn_and_wait( brq::stdin_string( "abcbd" ) |
                                          brq::capture_stdout | brq::capture_stderr,
                                         "sed", "s/b/x/g" );
            ASSERT( r );
            ASSERT_EQ( r.out(), "axcxd" );
            ASSERT_EQ( r.err(), "" );
        }

        TEST( in_lined )
        {
            auto r = brq::spawn_and_wait( brq::stdin_string( "abcbd\nebfg\n" ) |
                                          brq::capture_stdout | brq::capture_stderr,
                                          "sed", "s/b/x/g" );
            ASSERT( r );
            ASSERT_EQ( r.out(), "axcxd\nexfg\n" );
            ASSERT_EQ( r.err(), "" );
        }
    };

    struct test_pipethrough
    {
        using strs = std::vector< std::string >;

        TEST( lines )
        {
            strs out;
            brq::pipethrough( brq::pipe_feed( "foo\nbar\nbaz" ),
                              brq::pipe_read_lines( [&]( auto l ){ out.emplace_back( l ); }),
                              "cat" );
            ASSERT_EQ( out, strs{ "foo", "bar", "baz" } );
        }

        TEST( noread )
        {
            std::string out;
            brq::pipethrough( []( const brq::unique_fd & ){ return brq::io_result::done; },
                              brq::pipe_read_lines( [&]( auto l ){ out = l; }),
                              "echo", "-e", "lorem ipsum\\nhello world" );
            ASSERT_EQ( out, "hello world" );
        }

        TEST( empty )
        {
            int n = 0;
            brq::pipethrough( brq::pipe_feed( "this\ngets\ndiscarded" ),
                              brq::pipe_read_lines( [&]( auto ){ ++n; }),
                              "sh", "-c", "> /dev/null" );
            ASSERT_EQ( n, 0 );
        }

        TEST( retval )
        {
            int n = 0;
            int ret = brq::pipethrough( []( const brq::unique_fd & ){ return brq::io_result::done; },
                                        brq::pipe_read_lines( [&]( auto ){ ++n; }),
                                        "true" );
            ASSERT_EQ( ret, 0 );
            ASSERT_EQ( n, 0 );
            ret = brq::pipethrough( []( const brq::unique_fd & ){ return brq::io_result::done; },
                                    brq::pipe_read_lines( [&]( auto ){ ++n; }),
                                    "false" );
            ASSERT_LT( 0, ret );
            ASSERT_EQ( n, 0 );

            try {
                ret = brq::pipethrough( []( brq::unique_fd & ){ return brq::io_result::done; },
                                        brq::pipe_read_lines( [&]( auto ){ ++n; }),
                                        "/dev/null" );
            } catch ( ... ) {
                // this only happens in the child
                abort();
            }
            ASSERT_LT( ret, 0 );
            ASSERT_EQ( n, 0 );
        }
    };
}
#endif
