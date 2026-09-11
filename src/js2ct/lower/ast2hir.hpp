#pragma once

#include "../../common/error.hpp"
#include "../../common/visit.hpp"
#include "../ir/hir.hpp"
#include "../sema/analysis.hpp"

namespace qthu::js2ct::hir {
    struct lowerer {
        sema::analysis_result &sema;

        struct func_ctx {
            function fn;
        };

        expr_id append_expr(func_ctx &fc, expr e) {
            expr_id id{static_cast<uint32_t>(fc.fn.expressions.size())};
            fc.fn.expressions.push_back(std::move(e));
            return id;
        }

        stmt_id append_stmt(func_ctx &fc, stmt s) {
            stmt_id id{static_cast<uint32_t>(fc.fn.statements.size())};
            fc.fn.statements.push_back(std::move(s));
            return id;
        }

        expr_id lower_expr(func_ctx &fc, ast::expr &e) {
            expr res{.typ = type::jsvalue};

            std::visit(overloaded{
                           [ & ](ast::int_lit &il) {
                               res.data = expr::int_lit{.value = il.value};
                           },
                           [ & ](ast::bool_lit &bl) {
                               res.data = expr::bool_lit{.value = bl.value};
                           },
                           [ & ](ast::str_lit &sl) {
                               res.data = expr::str_lit{.value = sl.value};
                           },
                           [ & ](ast::var &) {
                               res.data = expr::var{.id = sema.identifier_bindings.at(&e)};
                           },
                           [ & ](ast::unary &u) {
                               expr_id sub = lower_expr(fc, *u.sub);
                               res.data = expr::unary{.op = u.op, .sub = sub};
                           },
                           [ & ](ast::binary &b) {
                               expr_id left = lower_expr(fc, *b.left);
                               expr_id right = lower_expr(fc, *b.right);
                               res.data = expr::binary{.op = b.op, .left = left, .right = right};
                           },
                           [ & ](ast::assign &a) {
                               if (std::holds_alternative<ast::var>(a.target)) {
                                   expr_id value = lower_expr(fc, *a.value);
                                   res.data = expr::assign{.target = sema.assign_bindings.at(&e), .value = value};
                               } else {
                                   auto &m = std::get<ast::member>(a.target);
                                   expr_id key = lower_expr(fc, *m.key);
                                   expr_id value = lower_expr(fc, *a.value);
                                   res.data = expr::member_assign{
                                       .object = sema.assign_bindings.at(&e),
                                       .key = key,
                                       .value = value,
                                   };
                               }
                           },
                           [ & ](ast::call &c) {
                               sema::function_id fid = sema.direct_calls.at(&e);
                               std::vector<expr_id> args;
                               for (auto &arg: c.args)
                                   args.push_back(lower_expr(fc, *arg));
                               res.data = expr::call{.target = fid, .args = std::move(args)};
                           },
                           [ & ](ast::member &m) {
                               expr_id object = lower_expr(fc, *m.object);
                               expr_id key = lower_expr(fc, *m.key);
                               res.data = expr::member{.object = object, .key = key};
                           },
                           [ & ](ast::object_lit &ol) {
                               std::vector<std::pair<std::string_view, expr_id> > props;
                               for (auto &[key, val]: ol.props)
                                   props.emplace_back(key, lower_expr(fc, *val));
                               res.data = expr::object_lit{.props = std::move(props)};
                           },
                           [ & ](ast::array_lit &al) {
                               std::vector<expr_id> elements;
                               for (auto &elem: al.elements)
                                   elements.push_back(lower_expr(fc, *elem));
                               res.data = expr::array_lit{.elements = std::move(elements)};
                           },
                       }, e.data);

            return append_expr(fc, std::move(res));
        }

        stmt_id make_block(func_ctx &fc, std::vector<stmt_id> stmts) {
            return append_stmt(fc, stmt{.data = stmt::block{std::move(stmts)}});
        }

        expr_id always_true(func_ctx &fc) {
            return append_expr(fc, expr{.typ = type::jsvalue, .data = expr::bool_lit{.value = true}});
        }

        std::optional<stmt_id> lower_stmt_opt(func_ctx &fc, ast::stmt &s, module &mod) {
            if (std::get_if<ast::fn_declaration>(&s.data)) {
                lower_function(s, fc.fn.id, mod);
                return std::nullopt;
            }
            return lower_stmt(fc, s, mod);
        }

        stmt_id lower_stmt(func_ctx &fc, ast::stmt &s, module &mod) {
            return std::visit(overloaded{
                                  [ & ](ast::block &b) -> stmt_id {
                                      std::vector<stmt_id> out;
                                      for (auto &stmt: b.stmts)
                                          if (auto id = lower_stmt_opt(fc, *stmt, mod))
                                              out.push_back(*id);

                                      return make_block(fc, std::move(out));
                                  },
                                  [ & ](ast::var_declaration &vd) -> stmt_id {
                                      std::vector<stmt_id> out;
                                      for (auto &dec: vd.declarators) {
                                          std::optional<expr_id> value;
                                          if (dec.init)
                                              value = lower_expr(fc, dec.init.value());

                                          sema::binding_id bid = sema.declarator_bindings.at(&dec);
                                          stmt::let_stmt ls{.typ = type::jsvalue, .target = bid, .value = value};
                                          out.push_back(append_stmt(fc, stmt{.data = std::move(ls)}));
                                      }

                                      return out.size() == 1 ? out[0] : make_block(fc, std::move(out));
                                  },
                                  [ & ](ast::fn_declaration &) -> stmt_id {
                                      error("unhandled statement kind in lowering");
                                      return stmt_id{.id = 69};
                                  },
                                  [ & ](ast::ret &r) -> stmt_id {
                                      std::optional<expr_id> val;
                                      if (r.value)
                                          val = lower_expr(fc, *r.value);
                                      return append_stmt(fc, stmt{.data = stmt::ret_stmt{val}});
                                  },
                                  [ & ](ast::if_stmt &i) -> stmt_id {
                                      expr_id cond = lower_expr(fc, i.cond);
                                      stmt_id then_b = lower_stmt(fc, *i.then_branch, mod);
                                      std::optional<stmt_id> else_b;
                                      if (i.else_branch)
                                          else_b = lower_stmt(fc, *i.else_branch, mod);
                                      return append_stmt(fc, stmt{.data = stmt::if_stmt{cond, then_b, else_b}});
                                  },
                                  [ & ](ast::do_while_stmt &dw) -> stmt_id {
                                      // do{body}while(cond) === body; while(cond){body} --
                                      // lower the body twice (source AST, not HIR, so this
                                      // is a plain re-walk, not aliasing) rather than giving
                                      // loop_stmt a second shape for post-condition loops.
                                      stmt_id first_body = lower_stmt(fc, *dw.body, mod);
                                      expr_id cond = lower_expr(fc, dw.cond);
                                      stmt_id loop_body = lower_stmt(fc, *dw.body, mod);
                                      stmt_id loop =
                                              append_stmt(fc, stmt{.data = stmt::loop_stmt{cond, loop_body}});
                                      return make_block(fc, {first_body, loop});
                                  },
                                  [ & ](ast::while_stmt &w) -> stmt_id {
                                      expr_id cond = lower_expr(fc, w.cond);
                                      stmt_id body = lower_stmt(fc, *w.body, mod);
                                      return append_stmt(fc, stmt{.data = stmt::loop_stmt{cond, body}});
                                  },
                                  [ & ](ast::for_stmt &fl) -> stmt_id {
                                      std::vector<stmt_id> outer;
                                      if (fl.init)
                                          outer.push_back(lower_stmt(fc, *fl.init, mod));

                                      expr_id cond = fl.cond ? lower_expr(fc, *fl.cond) : always_true(fc);

                                      std::vector<stmt_id> body_stmts;
                                      body_stmts.push_back(lower_stmt(fc, *fl.body, mod));
                                      if (fl.update)
                                          body_stmts.push_back(append_stmt(
                                              fc, stmt{.data = stmt::expr_stmt{lower_expr(fc, *fl.update)}}));

                                      stmt_id loop_body = make_block(fc, std::move(body_stmts));
                                      outer.push_back(
                                          append_stmt(fc, stmt{.data = stmt::loop_stmt{cond, loop_body}}));
                                      return make_block(fc, std::move(outer));
                                  },
                                  [ & ](ast::expr_stmt &es) -> stmt_id {
                                      // Recognized purely syntactically here, not via a call
                                      // to sema (which already knows not to resolve "assert"
                                      // as a declared function -- see analysis.hpp's call
                                      // case).
                                      if (auto *call = std::get_if<ast::call>(&es.value.data)) {
                                          if (auto *callee_var = std::get_if<ast::var>(&call->callee->data);
                                              callee_var && callee_var->name == "assert") {
                                              expr_id arg = lower_expr(fc, *call->args[0]);
                                              return append_stmt(fc, stmt{.data = stmt::assert_stmt{arg}});
                                          }
                                      }

                                      expr_id e = lower_expr(fc, es.value);
                                      return append_stmt(fc, stmt{.data = stmt::expr_stmt{e}});
                                  },
                                  [ & ](ast::brk &) -> stmt_id {
                                      return append_stmt(fc, stmt{.data = stmt::brk{}});
                                  },
                                  [ & ](ast::cont &) -> stmt_id {
                                      return append_stmt(fc, stmt{.data = stmt::cont{}});
                                  },
                              }, s.data);
        }

        void lower_function(ast::stmt &s, sema::function_id parent, module &mod) {
            func_ctx fc{};
            fc.fn.id = sema.stmt_functions.at(&s);
            fc.fn.lexical_parent = parent;

            std::vector<stmt_id> body_stmts;
            ast::fn_declaration &fd = std::get<ast::fn_declaration>(s.data);
            for (auto &param: fd.params)
                fc.fn.parameters.push_back(sema.param_bindings.at(&param));

            for (auto &sub: fd.body.stmts)
                if (auto id = lower_stmt_opt(fc, *sub, mod))
                    body_stmts.push_back(id.value());

            fc.fn.body_root = make_block(fc, std::move(body_stmts));
            mod.functions.push_back(std::move(fc.fn));
        }

        hir::module lower(ast::program &ast) {
            hir::module mod{};
            func_ctx fc{};
            fc.fn.id = sema.global_function;
            fc.fn.lexical_parent = std::nullopt;

            std::vector<stmt_id> top;
            for (auto &sub: ast.statements)
                if (auto id = lower_stmt_opt(fc, *sub, mod))
                    top.push_back(id.value());

            fc.fn.body_root = make_block(fc, std::move(top));
            mod.script = std::move(fc.fn);
            return mod;
        }
    };
}
