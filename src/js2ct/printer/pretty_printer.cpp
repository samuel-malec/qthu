#include <cassert>
#include <functional>

#include "pretty_printer.hpp"
#include "../../common/visit.hpp"

namespace qthu::js2ct::print {
    void pretty_printer::print_ast_expr(std::ostream &out, ast::expr &e, int depth) {
        pad(out, depth);

        std::visit(overloaded{
                       [ & ](ast::int_lit &lit) {
                           out << "[ int_lit ] " << lit.value << '\n';
                       },
                       [ & ](ast::bool_lit &lit) {
                           out << "[ bool_lit ] " << std::boolalpha << lit.value << '\n';
                       },
                       [ & ](ast::str_lit &lit) {
                           out << "[ str_lit ] \"" << lit.value << "\"\n";
                       },
                       [ & ](ast::var &id) {
                           out << "[ var ] " << id.name << '\n';
                       },
                       [ & ](ast::unary &u) {
                           out << "[ unary " << u.op << " ]\n";
                           print_ast_expr(out, *u.sub, depth + 1);
                       },
                       [ & ](ast::binary &b) {
                           out << "[ binary " << b.op << " ]\n";
                           print_ast_expr(out, *b.left, depth + 1);
                           print_ast_expr(out, *b.right, depth + 1);
                       },
                       [ & ](ast::assign &a) {
                           out << "[ assign ]\n";

                           pad(out, depth + 1);
                           out << "[ target ]\n";
                           if (auto *v = std::get_if<ast::var>(&a.target)) {
                               pad(out, depth + 2);
                               out << "[ var ] " << v->name << '\n';
                           } else {
                               auto &m = std::get<ast::member>(a.target);
                               print_ast_expr(out, *m.object, depth + 2);
                               print_ast_expr(out, *m.key, depth + 2);
                           }

                           pad(out, depth + 1);
                           out << "[ value ]\n";
                           print_ast_expr(out, *a.value, depth + 2);
                       },
                       [ & ](ast::call &c) {
                           out << "[ call ]\n";

                           pad(out, depth + 1);
                           out << "[ callee ]\n";
                           print_ast_expr(out, *c.callee, depth + 2);

                           pad(out, depth + 1);
                           out << "[ args ]\n";

                           for (auto &arg: c.args)
                               print_ast_expr(out, *arg, depth + 2);
                       },
                       [ & ](ast::member &m) {
                           out << "[ member" << (m.computed ? " computed" : "") << " ]\n";
                           print_ast_expr(out, *m.object, depth + 1);
                           print_ast_expr(out, *m.key, depth + 1);
                       },
                       [ & ](ast::object_lit &ol) {
                           out << "[ object_lit ]\n";
                           for (auto &[key, val]: ol.props) {
                               pad(out, depth + 1);
                               out << "[ prop ] " << key << '\n';
                               print_ast_expr(out, *val, depth + 2);
                           }
                       },
                       [ & ](ast::array_lit &al) {
                           out << "[ array_lit ]\n";
                           for (auto &elem: al.elements)
                               print_ast_expr(out, *elem, depth + 1);
                       },
                   }, e.data);
    }

    void pretty_printer::print_ast_stmt(std::ostream &out, ast::stmt &s, int depth) {
        pad(out, depth);

        std::visit(overloaded{
                       [ & ](ast::block &b) {
                           out << "[ block ]\n";

                           for (auto &stmt: b.stmts)
                               print_ast_stmt(out, *stmt, depth + 1);
                       },
                       [ & ](ast::var_declaration &vd) {
                           out << "[ var_declaration ]\n";

                           for (auto &decl: vd.declarators) {
                               pad(out, depth + 1);

                               switch (vd.kind) {
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

                               if (decl.init)
                                   print_ast_expr(out, *decl.init, depth + 2);
                           }
                       },
                       [ & ](ast::fn_declaration &fd) {
                           out << "[ fn_decl ]";
                           out << " " << fd.name << "( ";
                           for (size_t i = 0; i < fd.params.size(); ++i) {
                               auto &p = fd.params[i];
                               out << " " << p.name << (i == fd.params.size() - 1 ? "" : ", ");
                           }

                           out << " )\n";
                           for (auto &s: fd.body.stmts)
                               print_ast_stmt(out, *s, 1);
                       },
                       [ & ](ast::ret &r) {
                           out << "[ return ]";

                           if (r.value) {
                               out << '\n';
                               print_ast_expr(out, *r.value, depth + 1);
                           } else
                               out << '\n';
                       },
                       [ & ](ast::if_stmt &i) {
                           out << "[ if ]\n";
                           pad(out, depth + 1);
                           out << "[ condition ]\n";
                           print_ast_expr(out, i.cond, depth + 2);

                           pad(out, depth + 1);
                           out << "[ then ]\n";
                           print_ast_stmt(out, *i.then_branch, depth + 2);

                           if (i.else_branch) {
                               pad(out, depth + 1);
                               out << "[ else ]\n";
                               print_ast_stmt(out, *i.else_branch, depth + 2);
                           }
                       },
                       [ & ](ast::while_stmt &w) {
                           out << "[ while ]\n";

                           pad(out, depth + 1);
                           out << "[ condition ]\n";
                           print_ast_expr(out, w.cond, depth + 2);

                           print_ast_stmt(out, *w.body, depth + 1);
                       },
                       [ & ](ast::do_while_stmt &dw) {
                           out << "[ do_while ]\n";

                           pad(out, depth + 1);
                           out << "[ condition ]\n";
                           print_ast_expr(out, dw.cond, depth + 2);

                           print_ast_stmt(out, *dw.body, depth + 1);
                       },
                       [ & ](ast::for_stmt &fl) {
                           out << "[ for ]\n";

                           if (fl.init) {
                               pad(out, depth + 1);
                               out << "[ init ]\n";
                               print_ast_stmt(out, *fl.init, depth + 2);
                           }
                           if (fl.cond) {
                               pad(out, depth + 1);
                               out << "[ condition ]\n";
                               print_ast_expr(out, *fl.cond, depth + 2);
                           }
                           if (fl.update) {
                               pad(out, depth + 1);
                               out << "[ update ]\n";
                               print_ast_expr(out, *fl.update, depth + 2);
                           }

                           print_ast_stmt(out, *fl.body, depth + 1);
                       },
                       [ & ](ast::expr_stmt &e) {
                           out << "[ expr_stmt ]\n";
                           print_ast_expr(out, e.value, depth + 1);
                       },
                       [ & ](ast::brk &) {
                           out << "[ break ]\n";
                       },
                       [ & ](ast::cont &) {
                           out << "[ continue ]\n";
                       },
                   }, s.data);
    }

    void pretty_printer::print_ast(std::ostream &out, ast::program &ast) {
        out << "\n[AST]\n";
        for (auto &s: ast.statements)
            print_ast_stmt(out, *s, 1);
    }

    void pretty_printer::print_lin_value(std::ostream &out, const lin::value &v) {
        out << "%" << v.id;
    }

    void pretty_printer::print_lin_constant(std::ostream &out, const lin::constant &c) {
        std::visit(overloaded{
                       [ & ](uint64_t i) { out << i; },
                       [ & ](bool b) { out << (b ? "true" : "false"); },
                   }, c);
    }

    void pretty_printer::print_lin_argument(std::ostream &out, const lin::argument &arg) {
        std::visit(overloaded{
                       [ & ](const lin::constant &c) { print_lin_constant(out, c); },
                       [ & ](const lin::value &v) { print_lin_value(out, v); },
                   }, arg);
    }

    void pretty_printer::print_lin_instr(std::ostream &out, const lin::instr &i, int depth) {
        pad(out, depth);

        std::visit(overloaded{
                       [ & ](const lin::cons_data &ud) {
                           out << "[ cons ] ";
                           print_lin_value(out, ud.target);
                           out << " = ";
                           print_lin_constant(out, ud.c);
                           out << '\n';
                       },
                       [ & ](const lin::str_cons_data &sd) {
                           out << "[ cons ] ";
                           print_lin_value(out, sd.target);
                           out << " = \"" << sd.str << "\"\n";
                       },
                       [ & ](const lin::get_data &gd) {
                           out << "[ get ] ";
                           print_lin_value(out, gd.target);
                           out << " = ";
                           print_lin_argument(out, gd.obj);
                           out << "[";
                           print_lin_argument(out, gd.key);
                           out << "]\n";
                       },
                       [ & ](const lin::cons_obj_data &cod) {
                           out << "[ cons_obj ] ";
                           print_lin_value(out, cod.target);
                           out << " = {}\n";
                       },
                       [ & ](const lin::cons_arr_data &cad) {
                           out << "[ cons_arr ] ";
                           print_lin_value(out, cad.target);
                           out << " = []\n";
                       },
                       [ & ](const lin::set_data &sd) {
                           out << "[ set ] ";
                           print_lin_value(out, sd.target);
                           out << " = ";
                           print_lin_argument(out, sd.obj);
                           out << "[";
                           print_lin_argument(out, sd.key);
                           out << "] = ";
                           print_lin_argument(out, sd.val);
                           out << '\n';
                       },
                       [ & ](const lin::unary_data &u) {
                           out << "[ unary ] ";
                           print_lin_value(out, u.target);
                           out << " = ";

                           out << u.op << " ";
                           print_lin_argument(out, u.arg1);
                           out << '\n';
                       },
                       [ & ](const lin::binary_data &b) {
                           out << "[ binary ] ";
                           print_lin_value(out, b.target);
                           out << " = ";

                           print_lin_argument(out, b.arg1);
                           out << " " << b.op << " ";
                           print_lin_argument(out, b.arg2);

                           out << '\n';
                       },
                       [ & ](const lin::copy_data &c) {
                           out << "[ copy ] ";
                           print_lin_value(out, c.target);
                           out << " = ";
                           print_lin_argument(out, c.arg1);
                           out << '\n';
                       },
                       [ & ](const lin::dup_data &dd) {
                           out << "[ dup ] ";
                           print_lin_argument(out, dd.arg1);
                           out << " -> ";
                           print_lin_value(out, dd.first);
                           out << " , ";
                           print_lin_value(out, dd.second);
                           out << '\n';
                       },
                       [ & ](const lin::drop_data &dr) {
                           out << "[ drop ] ";
                           print_lin_value(out, dr.target);
                           out << '\n';
                       },
                       [ & ](const lin::call_data &c) {
                           out << "[ call ] ";
                           print_lin_value(out, c.target);
                           out << " = fn#" << c.callee.value << "( ";

                           for (size_t idx = 0; idx < c.args.size(); idx++) {
                               print_lin_argument(out, c.args[idx]);

                               if (idx + 1 != c.args.size())
                                   out << ", ";
                           }

                           out << " )\n";
                       },
                       [ & ](const lin::ret_data &r) {
                           out << "[ return ] ";

                           if (r.arg)
                               print_lin_argument(out, *r.arg);

                           out << '\n';
                       },
                       [ & ](const lin::assert_data &ad) {
                           out << "[ assert ] ";
                           print_lin_argument(out, ad.arg);
                           out << '\n';
                       },
                       [ & ](const lin::if_data &id) {
                           out << "[ if ]\n";

                           pad(out, depth + 1);
                           out << "[ condition ] ";
                           print_lin_argument(out, id.cond);
                           out << '\n';

                           pad(out, depth + 1);
                           out << "[ then ]\n";

                           for (auto &instr: id.then_body)
                               print_lin_instr(out, instr, depth + 2);

                           pad(out, depth + 1);
                           out << "[ then outputs ] ";
                           for (auto &v: id.then_outputs) {
                               print_lin_value(out, v);
                               out << ' ';
                           }
                           out << '\n';

                           pad(out, depth + 1);
                           out << "[ else ]\n";

                           for (auto &instr: id.else_body)
                               print_lin_instr(out, instr, depth + 2);

                           pad(out, depth + 1);
                           out << "[ else outputs ] ";
                           for (auto &v: id.else_outputs) {
                               print_lin_value(out, v);
                               out << ' ';
                           }
                           out << '\n';

                           pad(out, depth + 1);
                           out << "[ outputs ] ";
                           for (auto &v: id.outputs) {
                               print_lin_value(out, v);
                               out << ' ';
                           }
                           out << '\n';
                       },
                       [ & ](const lin::loop_data &l) {
                           out << "[ loop ]\n";

                           pad(out, depth + 1);
                           out << "[ cond ]\n";
                           for (auto &instr: l.cond_body)
                               print_lin_instr(out, instr, depth + 2);
                           pad(out, depth + 1);
                           out << "[ condition value ] ";
                           print_lin_argument(out, l.cond);
                           out << '\n';

                           pad(out, depth + 1);
                           out << "[ dispatch args ] ";
                           for (auto &v: l.dispatch_args) {
                               print_lin_value(out, v);
                               out << ' ';
                           }
                           out << '\n';

                           pad(out, depth + 1);
                           out << "[ body ]\n";
                           for (auto &instr: l.body)
                               print_lin_instr(out, instr, depth + 2);

                           pad(out, depth + 1);
                           out << "[ outputs ] ";
                           for (auto &v: l.outputs) {
                               print_lin_value(out, v);
                               out << ' ';
                           }
                           out << '\n';
                       },
                       [ & ](const lin::brk_data &) {
                           out << "[ break ]\n";
                       },
                       [ & ](const lin::cont_data &) {
                           out << "[ continue ]\n";
                       },
                   }, i.data);
    }

    void pretty_printer::print_lin_function(std::ostream &out, const lin::function &fn) {
        out << "[ function ] #" << fn.name.value << "\n";

        for (auto &instr: fn.body)
            print_lin_instr(out, instr, 1);
    }

    void pretty_printer::print_lin_program(std::ostream &out, const lin::program &p) {
        out << "\n[LIN]\n";

        for (auto &fn: p.functions)
            print_lin_function(out, fn);
    }

    void print_indent(std::ostream &out, int depth) {
        for (int i = 0; i < depth; ++i)
            out << "    ";
    }

    bool is_empty_block(hir::function &fn, hir::stmt_id s) {
        const auto *block = std::get_if<hir::stmt::block>(&fn.get(s).data);
        return block && block->stmts.empty();
    }

    void pretty_printer::print_hir_expr(std::ostream &out, hir::function &fn, hir::expr_id e, int depth) {
        const auto &node = fn.get(e);

        std::visit(overloaded{
                       [ & ](const hir::expr::int_lit &lit) {
                           out << "[int_lit:" << node.typ << "] " << lit.value;
                       },
                       [ & ](const hir::expr::bool_lit &lit) {
                           out << "[bool_lit:" << node.typ << "] " << (lit.value ? "true" : "false");
                       },
                       [ & ](const hir::expr::str_lit &lit) {
                           out << "[str_lit:" << node.typ << "] \"" << lit.value << "\"";
                       },
                       [ & ](const hir::expr::var &v) {
                           out << "[var:" << node.typ << "] " << "v" << v.id.value;
                       },
                       [ & ](const hir::expr::unary &u) {
                           out << "[unary:" << node.typ << "] " << u.op << "\n";
                           print_indent(out, depth + 1);
                           print_hir_expr(out, fn, u.sub, depth + 1);
                       },
                       [ & ](const hir::expr::binary &b) {
                           out << "[binary:" << node.typ << "] " << b.op << "\n";
                           print_indent(out, depth + 1);
                           print_hir_expr(out, fn, b.left, depth + 1);
                           out << "\n";
                           print_indent(out, depth + 1);
                           print_hir_expr(out, fn, b.right, depth + 1);
                       },
                       [ & ](const hir::expr::assign &a) {
                           out << "[assign:" << node.typ << "] v" << a.target.value << " =\n";
                           print_indent(out, depth + 1);
                           print_hir_expr(out, fn, a.value, depth + 1);
                       },
                       [ & ](const hir::expr::call &c) {
                           out << "[call:" << node.typ << "] " << c.target.value << "(";
                           if (c.args.empty()) {
                               out << ")";
                               return;
                           }
                           out << "\n";
                           for (size_t i = 0; i < c.args.size(); ++i) {
                               print_indent(out, depth + 1);
                               print_hir_expr(out, fn, c.args[i], depth + 1);
                               out << (i + 1 != c.args.size() ? ",\n" : "\n");
                           }
                           print_indent(out, depth);
                           out << ")";
                       },
                       [ & ](const hir::expr::member &m) {
                           out << "[member:" << node.typ << "]\n";
                           print_indent(out, depth + 1);
                           print_hir_expr(out, fn, m.object, depth + 1);
                           out << "\n";
                           print_indent(out, depth + 1);
                           print_hir_expr(out, fn, m.key, depth + 1);
                       },
                       [ & ](const hir::expr::object_lit &ol) {
                           out << "[object_lit:" << node.typ << "] {";
                           if (ol.props.empty()) {
                               out << "}";
                               return;
                           }
                           out << "\n";
                           for (size_t i = 0; i < ol.props.size(); ++i) {
                               print_indent(out, depth + 1);
                               out << ol.props[i].first << ": ";
                               print_hir_expr(out, fn, ol.props[i].second, depth + 1);
                               out << (i + 1 != ol.props.size() ? ",\n" : "\n");
                           }
                           print_indent(out, depth);
                           out << "}";
                       },
                       [ & ](const hir::expr::array_lit &al) {
                           out << "[array_lit:" << node.typ << "] [";
                           if (al.elements.empty()) {
                               out << "]";
                               return;
                           }
                           out << "\n";
                           for (size_t i = 0; i < al.elements.size(); ++i) {
                               print_indent(out, depth + 1);
                               print_hir_expr(out, fn, al.elements[i], depth + 1);
                               out << (i + 1 != al.elements.size() ? ",\n" : "\n");
                           }
                           print_indent(out, depth);
                           out << "]";
                       },
                       [ & ](const hir::expr::member_assign &ma) {
                           out << "[member_assign:" << node.typ << "] v" << ma.object.value << "[\n";
                           print_indent(out, depth + 1);
                           print_hir_expr(out, fn, ma.key, depth + 1);
                           out << "\n";
                           print_indent(out, depth);
                           out << "] =\n";
                           print_indent(out, depth + 1);
                           print_hir_expr(out, fn, ma.value, depth + 1);
                       },
                   }, node.data);
    }

    void pretty_printer::print_hir_stmt(std::ostream &out, hir::function &fn, hir::stmt_id s, int depth) {
        std::function<void(hir::stmt_id, int)> go = [ & ](hir::stmt_id s, int depth) {
            const auto &node = fn.get(s);

            std::visit(overloaded{
                           [ & ](const hir::stmt::expr_stmt &st) {
                               print_indent(out, depth);
                               out << "[expr_stmt] ";
                               print_hir_expr(out, fn, st.expr, depth);
                               out << ";\n";
                           },
                           [ & ](const hir::stmt::block &st) {
                               print_indent(out, depth);
                               out << "[block] {\n";
                               for (const auto &sub: st.stmts)
                                   go(sub, depth + 1);
                               print_indent(out, depth);
                               out << "}\n";
                           },
                           [ & ](const hir::stmt::let_stmt &st) {
                               print_indent(out, depth);
                               out << "[let_stmt:" << st.typ << "] let v" << st.target.value;
                               if (st.value) {
                                   out << " = ";
                                   print_hir_expr(out, fn, *st.value, depth);
                               }
                               out << ";\n";
                           },
                           [ & ](const hir::stmt::if_stmt &st) {
                               print_indent(out, depth);
                               out << "[if_stmt] if ( ";
                               print_hir_expr(out, fn, st.cond, depth);
                               out << " )\n";
                               go(st.then_branch, depth);
                               if (st.else_branch && !is_empty_block(fn, *st.else_branch)) {
                                   print_indent(out, depth);
                                   out << "[else]\n";
                                   go(*st.else_branch, depth);
                               }
                           },
                           [ & ](const hir::stmt::loop_stmt &st) {
                               print_indent(out, depth);
                               out << "[loop_stmt] loop\n";
                               go(st.body, depth);
                           },
                           [ & ](const hir::stmt::ret_stmt &st) {
                               print_indent(out, depth);
                               out << "[ret_stmt] return";
                               if (st.value) {
                                   out << " ";
                                   print_hir_expr(out, fn, *st.value, depth);
                               }
                               out << ";\n";
                           },
                           [ & ](const hir::stmt::brk &) {
                               print_indent(out, depth);
                               out << "[brk] break;\n";
                           },
                           [ & ](const hir::stmt::cont &) {
                               print_indent(out, depth);
                               out << "[cont] continue;\n";
                           },
                           [ & ](const hir::stmt::assert_stmt &st) {
                               print_indent(out, depth);
                               out << "[assert_stmt] assert( ";
                               print_hir_expr(out, fn, st.arg, depth);
                               out << " );\n";
                           },
                       }, node.data);
        };

        go(s, depth);
    }

    void pretty_printer::print_hir_function(std::ostream &out, hir::function &fn, sema::analysis_result &semantics) {
        out << semantics.function_name(fn.id) << " :: params: ( ";
        for (size_t i = 0; i < fn.parameters.size(); ++i)
            out << "v" << fn.parameters[i].value << (i + 1 == fn.parameters.size() ? "" : ", ");
        out << " )";

        if (fn.lexical_parent)
            out << "  parent: " << semantics.function_name(*fn.lexical_parent);

        if (!fn.captures.empty()) {
            out << "  captures: ( ";
            for (size_t i = 0; i < fn.captures.size(); ++i)
                out << "v" << fn.captures[i].value << (i + 1 == fn.captures.size() ? "" : ", ");
            out << " )";
        }
        out << "\n";

        const hir::stmt &s = fn.get(fn.body_root);
        assert(std::holds_alternative< hir::stmt::block >( s.data ));
        print_hir_stmt(out, fn, fn.body_root, 1);
    }

    void pretty_printer::print_hir(std::ostream &out, hir::module &mod, sema::analysis_result &semantics) {
        out << "\n[HIR]\n";
        print_hir_function(out, mod.script, semantics);
        out << "\n";

        for (auto &fn: mod.functions) {
            print_hir_function(out, fn, semantics);
            out << "\n";
        }
    }

    void print_names(std::ostream &out, const std::vector<cthu::name> &names) {
        for (size_t i = 0; i < names.size(); ++i) {
            if (i) out << " ";
            out << names[i];
        }
    }

    void print_insn(std::ostream &out, const cthu::insn &i) {
        out << "        " << i.structure << " " << i.operation;

        if (i.literal)
            out << " \"" << *i.literal << "\"";
        else if (!i.in.empty()) {
            out << " ";
            print_names(out, i.in);
        }
        if (!i.out.empty()) {
            out << " → ";
            print_names(out, i.out);
        }
        out << "\n";
    }

    void print_function(std::ostream &out, const cthu::name &fname, const cthu::function &fn) {
        out << "    " << fname << " = λ";

        if (!fn.in.empty()) {
            out << " ";
            print_names(out, fn.in);
        }
        if (!fn.out.empty()) {
            out << " → ";
            print_names(out, fn.out);
        }
        out << "\n    (\n";

        for (auto &i: fn.body)
            print_insn(out, i);

        out << "    )\n";
    }

    void print_structure(std::ostream &out, const cthu::structure &s) {
        out << "structure " << s.id << "\n(\n";

        for (auto &[fname, fn]: s.functions)
            print_function(out, fname, fn);

        out << ")\n";
    }

    void pretty_printer::print_cthu(std::ostream &out, const cthu::module &mod) {
        for (auto &s: mod.structures)
            qthu::js2ct::print::print_structure(out, s);
    }
}
