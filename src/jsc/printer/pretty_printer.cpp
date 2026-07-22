#include <functional>

#include "pretty_printer.hpp"

namespace jsc::print
{

using expr = ast::expr;
using stmt = ast::stmt;
using toplevel = ast::toplevel;
using var_decl = ast::var_decl;
using fn_decl = ast::fn_decl;

void pretty_printer::print_expr( std::ostream& out, expr& e, int depth )
{
    pad( out, depth );
    switch ( e.cat )
    {
        case expr::num_lit:
            out << "[ num_lit ] " << std::get< uint64_t >( e.val );
            break;
        case expr::bool_lit:
            out << "[ bool_lit ] " << std::get< bool >( e.val );
            break;
        case expr::identifier:
            out << "[ id ] " << e.id;
            break;
        case expr::unary:
            out << "[ unary " << e.op << " ]\n";
            print_expr( out, e[ 0 ], depth + 1 );
            return;
        case expr::binary:
            out << "[ binary " << e.op << " ]\n";
            print_expr( out, e[ 0 ], depth + 1 );
            print_expr( out, e[ 1 ], depth + 1 );
            return;
        case expr::relational:
            out << "[ rel " << e.op << " ]\n";
            print_expr( out, e[ 0 ], depth + 1 );
            print_expr( out, e[ 1 ], depth + 1 );
            return;
        case expr::assign:
            out << "[ assign ]\n";
            print_expr( out, e[ 0 ], depth + 1 );
            print_expr( out, e[ 1 ], depth + 1 );
            return;
        case expr::call:
            out << "[ call ]\n";
            print_expr( out, e.subs[ 0 ], depth + 1 );
            pad( out, depth + 1 );
            out << "[ params ]\n";
            for ( int i = 1; i < e.subs.size(); ++ i )
                print_expr( out, e.subs[ i ], depth + 2 );
            return;

        default:
            break;
    }
    out << '\n';
}

void pretty_printer::print_stmt( std::ostream& out, stmt& s, int depth )
{
    pad( out, depth );
    switch ( s.cat )
    {
        case stmt::ret:
            out << "[ ret ]";
            if  ( s.e.has_value() )
            {
                out << "\n";
                print_expr( out, s.e.value(), depth + 1 );
            }
            else
                out << " {}\n";
            return;
        
        case stmt::if_stmt:
            out << "[ if_stmt ]\n";
            pad( out, depth + 1 );
            out << "[ cond ]\n";
            if  ( s.e.has_value() )
                print_expr( out, s.e.value(), depth + 2 );
            else
                out << "{}";
            
            print_stmt( out, s[ 0 ], depth + 1 );
            if ( s.subs.size() > 1 )
            {
                pad( out, depth );
                out << "[ else ]\n";
                print_stmt( out, s[ 1 ], depth + 1 );
            }
            return;
        
        case stmt::for_stmt:
            out << "[ for_stmt ]\n";
            for ( auto& se : s.subs )
                print_stmt( out, se, depth + 1 );
            return;

        case stmt::while_stmt:
            out << "[ while_stmt ]\n";
            pad( out, depth );
            out << "[ cond ]\n";
            if ( !s.e.has_value() )
                out << " {} ";
            else
                print_expr( out, s.e.value(), depth );

            out << "\n";
            print_stmt( out, s[ 0 ], depth + 1 );
            return;

        case stmt::do_while_stmt:
            out << " [ do_while ]\n";
            print_stmt( out, s[ 0 ], depth + 1 );
            pad( out, depth );
            out << "[ cond ]\n";
            if ( !s.e.has_value() )
                out << " {} ";
            else
                print_expr( out, s.e.value(), depth );
            return;

        case stmt::cont:
            out << "[ continue ]";
            break;

        case stmt::brk:
            out << "[ break ]";
            break;

        case stmt::block:
            out << "[ block ]\n";
            for ( auto& se : s.subs )
                print_stmt( out, se, depth + 1 );
            return;

        case stmt::var_dclr:
            out << "[ var_decl ]\n";
            pad( out, depth + 1 );
            out << "[ ";
            switch ( s.vdecl.modifier )
            {
                case var_decl::mod_t::let:
                {
                    out << "let";
                    break;
                }
                case var_decl::mod_t::var:
                {
                    out << "var";
                    break;
                }
                case var_decl::mod_t::con:
                {
                    out << "const";
                    break; 
                }
                default:
                    break;
            }

            out << " ] " << s.vdecl.name << '\n';
            if ( s.vdecl.e.has_value() )
                print_expr( out, s.vdecl.e.value(), depth + 1 );
            return;

        case stmt::expr_stmt:
            out << "[ expr_stmt ]\n";
            if ( !s.e.has_value() )
                out << " {} ";
            else
                print_expr( out, s.e.value(), depth + 1 );
            return;

        default:
            break;
    }

    out << "\n";
}

void pretty_printer::print_ast( std::ostream& out, ast::program& ast )
{
    for ( auto& decl : ast.toplevel_items )
    {
        std::visit( [ this, &out ]( auto&& arg )
        {
            using T = std::decay_t< decltype( arg ) >;
            if constexpr ( std::is_same_v< T, fn_decl > )
            {
                out << "fn_decl: ";
                fn_decl fn = arg;
                out << " " << fn.name << "( ";
                for ( size_t i = 0; i < fn.params.size(); ++i )
                {
                    auto& p = fn.params[ i ];
                    out << " " << p.name << ( i == fn.params.size() - 1 ?  "" : ", " );
                }
                
                out << " )\n";
                for ( auto& s : fn.body )
                    print_stmt( out, s, 1 );
            }
            else if constexpr ( std::is_same_v< T, ast::stmt > )
            {
                stmt s = arg;
                print_stmt( out, s, 1 );
            }
            else
                static_assert( false, "non-exhaustive visitor!" );
        }, decl );
    }
}

template< typename... Ts >
struct overloaded : Ts... { using Ts::operator()...; };
template< typename... Ts >
overloaded( Ts... ) -> overloaded< Ts... >;

void print_indent( std::ostream& out, int depth )
{
    for ( int i = 0; i < depth; ++i )
        out << "    ";
}

bool is_empty_block( hir::function& fn, hir::stmt_id s )
{
    const auto* block = std::get_if< hir::stmt::block >( &fn.get( s ).data );
    return block && block->stmts.empty();
}

void pretty_printer::print_hir_expr( std::ostream& out, hir::function& fn, hir::expr_id e, sema::analysis_result& semantics )
{
    const auto& node = fn.get( e );

    std::visit( overloaded{
        [ & ]( const hir::expr::int_lit& lit )
        {
            out << lit.value;
        },
        [ & ]( const hir::expr::bool_lit& lit )
        {
            out << ( lit.value ? "true" : "false" );
        },
        [ & ]( const hir::expr::var& v )
        {
            out << "v" << v.id.id;
        },
        [ & ]( const hir::expr::unary& u )
        {
            out << "(" << u.op;
            print_hir_expr( out, fn, u.sub, semantics );
            out << ")";
        },
        [ & ]( const hir::expr::binary& b )
        {
            out << "(";
            print_hir_expr( out, fn, b.left, semantics );
            out << " " << b.op << " ";
            print_hir_expr( out, fn, b.right, semantics );
            out << ")";
        },
        [ & ]( const hir::expr::assign& a )
        {
            out << "v" << a.target.id << " = ";
            print_hir_expr( out, fn, a.value, semantics );
        },
        [ & ]( const hir::expr::call& c )
        {
            const auto& callee = semantics.functions.at( c.target.value );
            out << semantics.names.at( callee.name.value ) << "(";
            for ( size_t i = 0; i < c.args.size(); ++i )
            {
                print_hir_expr( out, fn, c.args[ i ], semantics );
                if ( i + 1 != c.args.size() )
                    out << ", ";
            }
            out << ")";
        },
    }, node.data );
}

void pretty_printer::print_hir_stmt( std::ostream& out, hir::function& fn, hir::stmt_id s, sema::analysis_result& semantics )
{
    std::function< void( hir::stmt_id, int ) > go = [ & ]( hir::stmt_id s, int depth )
    {
        const auto& node = fn.get( s );

        std::visit( overloaded{
            [ & ]( const hir::stmt::expr_stmt& st )
            {
                print_indent( out, depth );
                print_hir_expr( out, fn, st.expr, semantics );
                out << ";\n";
            },
            [ & ]( const hir::stmt::block& st )
            {
                print_indent( out, depth ); out << "{\n";
                for ( const auto& sub : st.stmts )
                    go( sub, depth + 1 );
                print_indent( out, depth ); out << "}\n";
            },
            [ & ]( const hir::stmt::let_stmt& st )
            {
                print_indent( out, depth );
                out << "let v" << st.target.id << " = ";
                print_hir_expr( out, fn, st.value, semantics );
                out << ";\n";
            },
            [ & ]( const hir::stmt::if_stmt& st )
            {
                print_indent( out, depth );
                out << "if ( ";
                print_hir_expr( out, fn, st.cond, semantics );
                out << " )\n";
                go( st.then_branch, depth );
                if ( !is_empty_block( fn, st.else_branch ) )
                {
                    print_indent( out, depth ); out << "else\n";
                    go( st.else_branch, depth );
                }
            },
            [ & ]( const hir::stmt::loop_stmt& st )
            {
                print_indent( out, depth ); out << "loop\n";
                go( st.body, depth );
            },
            [ & ]( const hir::stmt::ret_stmt& st )
            {
                print_indent( out, depth );
                out << "return ";
                print_hir_expr( out, fn, st.value, semantics );
                out << ";\n";
            },
            [ & ]( const hir::stmt::brk& )
            {
                print_indent( out, depth ); out << "break;\n";
            },
            [ & ]( const hir::stmt::cont& )
            {
                print_indent( out, depth ); out << "continue;\n";
            },
        }, node.data );
    };

    go( s, 1 );
}

void pretty_printer::print_hir( std::ostream& out, hir::program& hir, sema::analysis_result& semantics ) 
{
    for ( auto& fn : hir.functions )
    {
        out << semantics.names.at( fn.name.value ) << " ::  params: ( ";
        for ( size_t i = 0; i < fn.params.size(); ++i )
            out << "v" << fn.params[ i ].id << ( i == fn.params.size() - 1 ? "" : ". " );
        out << " )\n";
        print_hir_stmt( out, fn, fn.body_root, semantics );
    }    
}

void pretty_printer::print_cthu( std::ostream& out, const qthu::cthuc::module_t& mod )
{}

}
