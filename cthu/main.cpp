#include <brick-cmd>
#include <brick-trace>

#include "exec.hpp"
#include "reader.hpp"

namespace cthu
{
    /* stolen from tiny, TODO merge into bricks or something */

    struct cmd_file : brq::cmd_file
    {
        static brq::parse_result from_string( std::string_view s, cmd_file &f )
        {
            if ( s == "-" )
            {
                f.name = s;
                return {};
            }
            return brq::cmd_file::from_string( s, f );
        }

        std::string read()
        {
            return brq::read_file( name );
        }
    };

    template< typename cmd_t >
    inline int parse_and_run_single( int argc, char** argv )
    {
        brq::singleton< brq::assert_config >().quiet = true;
        brq::cmd_option_parser p( argc, argv, "" );
        std::unique_ptr< brq::cmd_base > cmd;

        auto move = [&cmd]( auto &&c )
        {
            using type = std::remove_cvref_t< decltype( c ) >;
            cmd.reset( new type( std::move( c ) ) );
        };

        try
        {
            p.opt_parse< cmd_t >().match( move );
            cmd->run();
        }
        catch ( std::exception &e )
        {
            std::cerr << "\033[1;31mexception:\033[m " << e.what() << "\n";
            return 1;
        }

        return 0;
    }

    struct command : brq::cmd_base
    {
        std::vector< cmd_file > _files;
        brq::cmd_flag _debug;

        void options( brq::cmd_options &c ) override
        {
            brq::cmd_base::options( c );
            c.opt( "--debug", _debug ) << "print what is being done";
            c.collect( _files );
        }

        std::string_view help() override
        {
            return "the cthu substitution-friendly virtual machine";
        }

        void run() override
        {
            if ( _debug )
                brq::trace().add_rule( "+", "debug" );

            if ( _files.empty() )
                _files.push_back( { "-" } );

            std::vector< reader::source_ptr > srcs;
            reader::symtab prog;

            for ( auto f : _files )
            {
                auto src = brq::make_refcount< reader::source_file >( f.name, f.read() );
                srcs.push_back( src );
                reader::reader r( src, prog );

                if ( auto err = r.parse() )
                {
                    brq::string_builder out;
                    err->print( out );
                    std::cerr << out.data();

                    if ( err->is_fatal() )
                        brq::raise() << "error parsing input";
                }
            }
        }
    };

    std::string cmd_name( command & ) { return ""; }
}

int main( int argc, char **argv )
{
    return cthu::parse_and_run_single< cthu::command >( argc, argv );
}
