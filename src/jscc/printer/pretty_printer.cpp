#include "pretty_printer.hpp"

namespace jscc::print
{

using expr = ast::expr;
using stmt = ast::stmt;
using toplevel = ast::toplevel;
using var_decl = ast::var_decl;
using fn_decl = ast::fn_decl;

void pretty_printer::print_expr( expr& e, int depth )
{
    pad( depth );
    switch ( e.cat )
    {
        case expr::num_lit:
            std::cout << "[ num_lit ] " << std::get< uint64_t >( e.val );
            break;
        case expr::bool_lit:
            std::cout << "[ bool_lit ] " << std::get< bool >( e.val );
            break;
        case expr::identifier:
            std::cout << "[ id ] " << e.id;
            break;
        case expr::unary:
            std::cout << "[ unary " << e.op << " ]\n";
            print_expr( e[ 0 ], depth + 1 );
            return;
        case expr::binary:
            std::cout << "[ binary " << e.op << " ]\n";
            print_expr( e[ 0 ], depth + 1 );
            print_expr( e[ 1 ], depth + 1 );
            return;
        case expr::relational:
            std::cout << "[ rel " << e.op << " ]\n";
            print_expr( e[ 0 ], depth + 1 );
            print_expr( e[ 1 ], depth + 1 );
            return;
        case expr::assign:
            std::cout << "[ assign ]\n";
            print_expr( e[ 0 ], depth + 1 );
            print_expr( e[ 1 ], depth + 1 );
            return;
        case expr::call:
            std::cout << "[ call ]\n";
            print_expr( e.subs[ 0 ], depth + 1 );
            pad( depth + 1 );
            std::cout << "[ params ]\n";
            for ( int i = 1; i < e.subs.size(); ++ i )
                print_expr( e.subs[ i ], depth + 2 );
            return;

        default:
            break;
    }
    std::cout << '\n';
}

void pretty_printer::print_stmt( stmt& s, int depth )
{
    pad( depth );
    switch ( s.cat )
    {
        case stmt::ret:
            std::cout << "[ ret ]";
            if  ( s.e.has_value() )
            {
                std::cout << "\n";
                print_expr( s.e.value(), depth + 1 );
            }
            else
                std::cout << " {}\n";
            return;
        
        case stmt::if_stmt:
            std::cout << "[ if_stmt ]\n";
            pad( depth + 1 );
            std::cout << "[ cond ]\n";
            if  ( s.e.has_value() )
                print_expr( s.e.value(), depth + 2 );
            else
                std::cout << "{}";
            
            print_stmt( s[ 0 ], depth + 1 );
            if ( s.subs.size() > 1 )
            {
                pad( depth );
                std::cout << "[ else ]\n";
                print_stmt( s[ 1 ], depth + 1 );
            }
            return;
        
        case stmt::for_stmt:
            std::cout << "[ for_stmt ]\n";
            for ( auto& se : s.subs )
                print_stmt( se, depth + 1 );
            return;

        case stmt::while_stmt:
            std::cout << "[ while_stmt ]\n";
            pad( depth );
            std::cout << "[ cond ]\n";
            if ( !s.e.has_value() )
                std::cout << " {} ";
            else
                print_expr( s.e.value(), depth );

            std::cout << "\n";
            print_stmt( s[ 0 ], depth + 1 );
            return;

        case stmt::do_while_stmt:
            std::cout << " [ do_while ]\n";
            print_stmt( s[ 0 ], depth + 1 );
            pad( depth );
            std::cout << "[ cond ]\n";
            if ( !s.e.has_value() )
                std::cout << " {} ";
            else
                print_expr( s.e.value(), depth );
            return;

        case stmt::cont:
            std::cout << "[ continue ]";
            break;

        case stmt::brk:
            std::cout << "[ break ]";
            break;

        case stmt::block:
            std::cout << "[ block ]\n";
            for ( auto& se : s.subs )
                print_stmt( se, depth + 1 );
            return;

        case stmt::var_dclr:
            std::cout << "[ var_decl ]\n";
            pad( depth + 1 );
            // FIXME: instead of putting 'var' here, we will have to put the actual declaration keyword instead 
            std::cout << "[ var ] " << " " << s.vdecl.name << '\n';
            if ( s.vdecl.e.has_value() )
                print_expr( s.vdecl.e.value(), depth + 1 );
            return;

        case stmt::expr_stmt:
            std::cout << "[ expr_stmt ]\n";
            if ( !s.e.has_value() )
                std::cout << " {} ";
            else
                print_expr( s.e.value(), depth + 1 );
            return;

        default:
            break;
    }

    std::cout << "\n";
}

void pretty_printer::print_ast( ast::program& ast )
{
    for ( auto& decl : ast.toplevel_items )
    {
        std::visit( [ this ]( auto&& arg )
        {
            using T = std::decay_t< decltype( arg ) >;
            if constexpr ( std::is_same_v< T, fn_decl > )
            {
                std::cout << "fn_decl: ";
                fn_decl fn = arg;
                std::cout << " " << fn.name << "( ";
                for ( size_t i = 0; i < fn.params.size(); ++i )
                {
                    auto& p = fn.params[ i ];
                    std::cout << " " << p.name << ( i == fn.params.size() - 1 ?  "" : ", " );
                }
                
                std::cout << " )\n";
                for ( auto& s : fn.body )
                    print_stmt( s, 1 );
            }
            else if constexpr ( std::is_same_v< T, ast::stmt > )
            {
                stmt s = arg;
                print_stmt( s, 1 );
            }
            else if constexpr( std::is_same_v< T, ast::var_decl > )
            {
                var_decl vd = arg;
                std::cout << "[ var_decl ]\n";
                // FIXME: instead of putting 'var' here, we will have to put the actual declaration keyword instead 
                std::cout << "[ var ] " << " " << vd.name << '\n';
                if ( vd.e.has_value() )
                    print_expr( vd.e.value(), 1 );
            }
            else
                static_assert( false, "non-exhaustive visitor!" );
        }, decl );
    }
}

};
