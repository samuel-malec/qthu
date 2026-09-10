#include <cassert>
#include <functional>

#include "pretty_printer.hpp"

namespace qthu::js2ct::print
{

void pretty_printer::print_ast_expr( std::ostream& out, ast::expr& e, int depth )
{
    pad( out, depth );

    if ( auto* lit = std::get_if< ast::int_lit >( &e.data ) )
        out << "[ int_lit ] " << lit->value << '\n';
    else if ( auto* lit = std::get_if< ast::bool_lit>( &e.data ) )
        out << "[ bool_lit ] " << std::boolalpha << lit->value << '\n';
    else if ( auto* id = std::get_if< ast::var >( &e.data ) )
        out << "[ var ] " << id->name << '\n';
    else if ( auto* u = std::get_if< ast::unary >( &e.data ) )
    {
        out << "[ unary " << u->op << " ]\n";
        print_ast_expr( out, *u->sub, depth + 1 );
    }
    else if ( auto* b = std::get_if< ast::binary >( &e.data ) )
    {
        out << "[ binary " << b->op << " ]\n";
        print_ast_expr( out, *b->left, depth + 1 );
        print_ast_expr( out, *b->right, depth + 1 );
    }
    else if ( auto* a = std::get_if< ast::assign >( &e.data ) )
    {
        out << "[ assign ]\n";

        pad( out, depth + 1 );
        out << "[ target ] " << a->target.name << '\n';

        pad( out, depth + 1 );
        out << "[ value ]\n";
        print_ast_expr( out, *a->value, depth + 2 );
    }
    else if ( auto* c = std::get_if< ast::call >( &e.data ) )
    {
        out << "[ call ]\n";

        pad( out, depth + 1 );
        out << "[ callee ]\n";
        print_ast_expr( out, *c->callee, depth + 2 );

        pad( out, depth + 1 );
        out << "[ args ]\n";

        for ( auto& arg : c->args )
            print_ast_expr( out, *arg, depth + 2 );
    }
}

void pretty_printer::print_ast_stmt( std::ostream& out, ast::stmt& s, int depth )
{
    pad( out, depth );

    if ( auto* b = std::get_if< ast::block >( &s.data ) )
    {
        out << "[ block ]\n";

        for ( auto& stmt : b->stmts )
            print_ast_stmt( out, *stmt, depth + 1 );
    }
    else if ( auto* vd = std::get_if< ast::var_declaration >( &s.data ) )
    {
        out << "[ var_declaration ]\n";

        for ( auto& decl : vd->declarators )
        {
            pad( out, depth + 1 );

            switch ( vd->kind )
            {
                case ast::var_declaration::kind_t::let:
                    out << "[ let ] ";
                    break;

                case ast::var_declaration::kind_t::var:
                    out << "[ var ] ";
                    break;

                case ast::var_declaration::kind_t::constant:
                    out << "[ const ] ";
                    break;
            }

            out << decl.name << '\n';

            if ( decl.init )
                print_ast_expr( out, *decl.init, depth + 2 );
        }
    }
    else if ( auto* fd = std::get_if< ast::fn_declaration >( &s.data ) )
    {
        out << "[ fn_decl ]";
        out << " " << fd->name << "( ";
        for ( size_t i = 0; i < fd->params.size(); ++i )
        {
            auto& p = fd->params[ i ];
            out << " " << p.name << ( i == fd->params.size() - 1 ?  "" : ", " );
        }
        
        out << " )\n";
        for ( auto& s : fd->body.stmts )
            print_ast_stmt( out, *s, 1 );
    }
    else if ( auto* r = std::get_if< ast::ret >( &s.data ) )
    {
        out << "[ return ]";

        if ( r->value )
        {
            out << '\n';
            print_ast_expr( out, *r->value, depth + 1 );
        }
        else
            out << '\n';
    }
    else if ( auto* i = std::get_if< ast::if_stmt >( &s.data ) )
    {
        out << "[ if ]\n";
        pad( out, depth + 1 );
        out << "[ condition ]\n";
        print_ast_expr( out, i->cond, depth + 2 );

        pad( out, depth + 1 );
        out << "[ then ]\n";
        print_ast_stmt( out, *i->then_branch, depth + 2 );

        if ( i->else_branch )
        {
            pad( out, depth + 1 );
            out << "[ else ]\n";
            print_ast_stmt( out, *i->else_branch, depth + 2 );
        }
    }
    else if ( auto* w = std::get_if< ast::while_stmt >( &s.data ) )
    {
        out << "[ while ]\n";

        pad( out, depth + 1 );
        out << "[ condition ]\n";
        print_ast_expr( out, w->cond, depth + 2 );

        print_ast_stmt( out, *w->body, depth + 1 );
    }
    else if ( auto* e = std::get_if< ast::expr_stmt >( &s.data ) )
    {
        out << "[ expr_stmt ]\n";
        print_ast_expr( out, e->value, depth + 1 );
    }
    else if ( std::get_if< ast::brk >( &s.data ) )
        out << "[ break ]\n";
    else if ( std::get_if< ast::cont >( &s.data ) )
        out << "[ continue ]\n";
}

void pretty_printer::print_ast( std::ostream& out, ast::program& ast )
{
    out << "\n[AST]\n";
    for ( auto& s : ast.statements )
        print_ast_stmt( out, *s, 1 );
}

void pretty_printer::print_lin_value( std::ostream& out, const lin::value& v )
{
    out << "%" << v.id;
}

void pretty_printer::print_lin_constant( std::ostream& out, const lin::constant& c )
{
    if ( auto* i = std::get_if< uint64_t >( &c ) )
        out << *i;
    else if ( auto* b = std::get_if< bool >( &c ) )
        out << ( *b ? "true" : "false" );
}

void pretty_printer::print_lin_argument( std::ostream& out, const lin::argument& arg )
{
    if ( auto* c = std::get_if< lin::constant >( &arg ) )
        print_lin_constant( out, *c );
    else if ( auto* v = std::get_if< lin::value >( &arg ) )
        print_lin_value( out, *v );
}

void pretty_printer::print_lin_instr( std::ostream& out, const lin::instr& i, int depth )
{
    pad( out, depth );
    if ( auto* ud = std::get_if< lin::cons_data >( &i.data ) )
    {
        out << "[ cons ] ";
        print_lin_value( out, ud->target );
        out << " = ";
        print_lin_constant( out, ud->c );
        out << '\n';
    }
    else if ( auto* sd = std::get_if< lin::str_cons_data >( &i.data ) )
    {
        out << "[ cons ] ";
        print_lin_value( out, sd->target );
        out << " = \"" << sd->str << "\"\n";
    }
    else if ( auto* u = std::get_if< lin::unary_data >( &i.data ) )
    {
        out << "[ unary ] ";
        print_lin_value( out, u->target );
        out << " = ";

        out << u->op << " ";
        print_lin_argument( out, u->arg1 );
        out << '\n';
    }
    else if ( auto* b = std::get_if< lin::binary_data >( &i.data ) )
    {
        out << "[ binary ] ";
        print_lin_value( out, b->target );
        out << " = ";

        print_lin_argument( out, b->arg1 );
        out << " " << b->op << " ";
        print_lin_argument( out, b->arg2 );

        out << '\n';
    }
    else if ( auto* c = std::get_if< lin::copy_data >( &i.data ) )
    {
        out << "[ copy ] ";
        print_lin_value( out, c->target );
        out << " = ";
        print_lin_argument( out, c->arg1 );
        out << '\n';
    }
    else if ( auto* dd = std::get_if< lin::dup_data >( &i.data ) )
    {
        out << "[ dup ] ";
        print_lin_argument( out, dd->arg1 );
        out << " -> ";
        print_lin_value( out, dd->first );
        out << " , ";
        print_lin_value( out, dd->second );
        out << '\n';
    }
    else if ( auto* dr = std::get_if< lin::drop_data >( &i.data ) )
    {
        out << "[ drop ] ";
        print_lin_value( out, dr->target );
        out << '\n';
    }
    else if ( auto* c = std::get_if< lin::call_data >( &i.data ) )
    {
        out << "[ call ] ";
        print_lin_value( out, c->target );
        out << " = fn#" << c->callee.value << "( ";

        for ( size_t idx = 0; idx < c->args.size(); idx++ )
        {
            print_lin_argument( out, c->args[ idx ] );

            if ( idx + 1 != c->args.size() )
                out << ", ";
        }

        out << " )\n";
    }
    else if ( auto* r = std::get_if< lin::ret_data >( &i.data ) )
    {
        out << "[ return ] ";

        if ( r->arg )
            print_lin_argument( out, *r->arg );

        out << '\n';
    }
    else if ( auto* id = std::get_if< lin::if_data >( &i.data ) )
    {
        out << "[ if ]\n";

        pad( out, depth + 1 );
        out << "[ condition ] ";
        print_lin_argument( out, id->cond );
        out << '\n';

        pad( out, depth + 1 );
        out << "[ then ]\n";

        for ( auto& instr : id->then_body )
            print_lin_instr( out, instr, depth + 2 );

        pad( out, depth + 1 );
        out << "[ else ]\n";

        for ( auto& instr : id->else_body )
            print_lin_instr( out, instr, depth + 2 );

        if ( id->result )
        {
            pad( out, depth + 1 );
            out << "[ result ] ";
            print_lin_value( out, *id->result );
            out << '\n';
        }
    }
    else if ( auto* l = std::get_if< lin::loop_data >( &i.data ) )
    {
        out << "[ loop ]\n";

        pad( out, depth + 1 );
        out << "[ cond ]\n";
        for ( auto& instr : l->cond_body )
            print_lin_instr( out, instr, depth + 2 );
        pad( out, depth + 1 );
        out << "[ condition value ] ";
        print_lin_argument( out, l->cond );
        out << '\n';

        pad( out, depth + 1 );
        out << "[ dispatch args ] ";
        for ( auto& v : l->dispatch_args )
        {
            print_lin_value( out, v );
            out << ' ';
        }
        out << '\n';

        pad( out, depth + 1 );
        out << "[ body ]\n";
        for ( auto& instr : l->body )
            print_lin_instr( out, instr, depth + 2 );

        pad( out, depth + 1 );
        out << "[ outputs ] ";
        for ( auto& v : l->outputs )
        {
            print_lin_value( out, v );
            out << ' ';
        }
        out << '\n';
    }
    else if ( std::get_if< lin::brk_data >( &i.data ) )
    {
        out << "[ break ]\n";
    }
    else if ( std::get_if< lin::cont_data >( &i.data ) )
    {
        out << "[ continue ]\n";
    }
    else
        assert( false && "unimplemented\n" );
}

void pretty_printer::print_lin_function( std::ostream& out, const lin::function& fn )
{
    out << "[ function ] #" << fn.name.value << "\n";

    for ( auto& instr : fn.body )
        print_lin_instr( out, instr, 1 );
}

void pretty_printer::print_lin_program( std::ostream& out, const lin::program& p )
{
    out << "\n[LIN]\n";

    for ( auto& fn : p.functions )
        print_lin_function( out, fn );
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

void pretty_printer::print_hir_expr( std::ostream& out, hir::function& fn, hir::expr_id e, int depth )
{
    const auto& node = fn.get( e );

    std::visit( overloaded{
        [ & ]( const hir::expr::int_lit& lit )
        {
            out << "[int_lit:" << node.typ << "] " << lit.value;
        },
        [ & ]( const hir::expr::bool_lit& lit )
        {
            out << "[bool_lit:" << node.typ << "] " << ( lit.value ? "true" : "false" );
        },
        [ & ]( const hir::expr::str_lit& lit )
        {
            out << "[str_lit:" << node.typ << "] \"" << lit.value << "\"";
        },
        [ & ]( const hir::expr::var& v )
        {
            out << "[var:" << node.typ << "] " << "v" << v.id.value;
        },
        [ & ]( const hir::expr::unary& u )
        {
            out << "[unary:" << node.typ << "] " << u.op << "\n";
            print_indent( out, depth + 1 );
            print_hir_expr( out, fn, u.sub, depth + 1 );
        },
        [ & ]( const hir::expr::binary& b )
        {
            out << "[binary:" << node.typ << "] " << b.op << "\n";
            print_indent( out, depth + 1 );
            print_hir_expr( out, fn, b.left, depth + 1 );
            out << "\n";
            print_indent( out, depth + 1 );
            print_hir_expr( out, fn, b.right, depth + 1 );
        },
        [ & ]( const hir::expr::assign& a )
        {
            out << "[assign:" << node.typ << "] v" << a.target.value << " =\n";
            print_indent( out, depth + 1 );
            print_hir_expr( out, fn, a.value, depth + 1 );
        },
        [ & ]( const hir::expr::call& c )
        {
            out << "[call:" << node.typ << "] " << c.target.value << "(";
            if ( c.args.empty() )
            {
                out << ")";
                return;
            }
            out << "\n";
            for ( size_t i = 0; i < c.args.size(); ++i )
            {
                print_indent( out, depth + 1 );
                print_hir_expr( out, fn, c.args[ i ], depth + 1 );
                out << ( i + 1 != c.args.size() ? ",\n" : "\n" );
            }
            print_indent( out, depth );
            out << ")";
        },
    }, node.data );
}

void pretty_printer::print_hir_stmt( std::ostream& out, hir::function& fn, hir::stmt_id s, int depth )
{
    std::function< void( hir::stmt_id, int ) > go = [ & ]( hir::stmt_id s, int depth )
    {
        const auto& node = fn.get( s );

        std::visit( overloaded{
            [ & ]( const hir::stmt::expr_stmt& st )
            {
                print_indent( out, depth );
                out << "[expr_stmt] ";
                print_hir_expr( out, fn, st.expr, depth );
                out << ";\n";
            },
            [ & ]( const hir::stmt::block& st )
            {
                print_indent( out, depth ); out << "[block] {\n";
                for ( const auto& sub : st.stmts )
                    go( sub, depth + 1 );
                print_indent( out, depth ); out << "}\n";
            },
            [ & ]( const hir::stmt::let_stmt& st )
            {
                print_indent( out, depth );
                out << "[let_stmt:" << st.typ << "] let v" << st.target.value;
                if ( st.value )
                {
                    out << " = ";
                    print_hir_expr( out, fn, *st.value, depth );
                }
                out << ";\n";
            },
            [ & ]( const hir::stmt::if_stmt& st )
            {
                print_indent( out, depth );
                out << "[if_stmt] if ( ";
                print_hir_expr( out, fn, st.cond, depth );
                out << " )\n";
                go( st.then_branch, depth );
                if ( st.else_branch && !is_empty_block( fn, *st.else_branch ) )
                {
                    print_indent( out, depth ); out << "[else]\n";
                    go( *st.else_branch, depth );
                }
            },
            [ & ]( const hir::stmt::loop_stmt& st )
            {
                print_indent( out, depth ); out << "[loop_stmt] loop\n";
                go( st.body, depth );
            },
            [ & ]( const hir::stmt::ret_stmt& st )
            {
                print_indent( out, depth );
                out << "[ret_stmt] return";
                if ( st.value )
                {
                    out << " ";
                    print_hir_expr( out, fn, *st.value, depth );
                }
                out << ";\n";
            },
            [ & ]( const hir::stmt::brk& )
            {
                print_indent( out, depth ); out << "[brk] break;\n";
            },
            [ & ]( const hir::stmt::cont& )
            {
                print_indent( out, depth ); out << "[cont] continue;\n";
            },
        }, node.data );
    };

    go( s, depth );
}

void pretty_printer::print_hir_function( std::ostream& out, hir::function& fn, sema::analysis_result& semantics )
{
    out << semantics.function_name( fn.id ) << " :: params: ( ";
    for ( size_t i = 0; i < fn.parameters.size(); ++i )
        out << "v" << fn.parameters[ i ].value << ( i + 1 == fn.parameters.size() ? "" : ", " );
    out << " )";

    if ( fn.lexical_parent )
        out << "  parent: " << semantics.function_name( *fn.lexical_parent );

    if ( !fn.captures.empty() )
    {
        out << "  captures: ( ";
        for ( size_t i = 0; i < fn.captures.size(); ++i )
            out << "v" << fn.captures[ i ].value << ( i + 1 == fn.captures.size() ? "" : ", " );
        out << " )";
    }
    out << "\n";

    const hir::stmt& s = fn.get( fn.body_root );
    assert( std::holds_alternative< hir::stmt::block >( s.data ) );
    print_hir_stmt( out, fn, fn.body_root, 1 );
}

void pretty_printer::print_hir( std::ostream& out, hir::module& mod, sema::analysis_result& semantics )
{
    out << "\n[HIR]\n"; 
    print_hir_function( out, mod.script, semantics );
    out << "\n";

    for ( auto& fn : mod.functions )
    {
        print_hir_function( out, fn, semantics );
        out << "\n";
    }
}

void print_names( std::ostream& out, const std::vector< cthu::name >& names )
{
    for( size_t i = 0; i < names.size(); ++i )
    {
        if( i ) out << " ";
        out << names[ i ];
    }
}

void print_insn( std::ostream& out, const cthu::insn& i )
{
    out << "        " << i.structure << " " << i.operation;

    if( i.literal )
        out << " \"" << *i.literal << "\"";
    else if( !i.in.empty() )
    {
        out << " ";
        print_names( out, i.in );
    }
    if( !i.out.empty() )
    {
        out << " → ";
        print_names( out, i.out );
    }
    out << "\n";
}

void print_function( std::ostream& out, const cthu::name& fname, const cthu::function& fn )
{
    out << "    " << fname << " = λ";

    if( !fn.in.empty() )
    {
        out << " ";
        print_names( out, fn.in );
    }
    if( !fn.out.empty() )
    {
        out << " → ";
        print_names( out, fn.out );
    }
    out << "\n    (\n";

    for( auto& i : fn.body )
        print_insn( out, i );

    out << "    )\n";
}

void print_structure( std::ostream& out, const cthu::structure& s )
{
    out << "structure " << s.id << "\n(\n";

    for( auto& [ fname, fn ] : s.functions )
        print_function( out, fname, fn );

    out << ")\n";
}

void pretty_printer::print_cthu( std::ostream& out, const cthu::module& mod )
{
    for( auto& s : mod.structures )
        qthu::js2ct::print::print_structure( out, s );
}

}
