#pragma once

#include "../../common/error.hpp"
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

            if (auto *il = std::get_if<ast::int_lit>(&e.data))
                res.data = expr::int_lit{.value = il->value};

            else if (auto *bl = std::get_if<ast::bool_lit>(&e.data))
                res.data = expr::bool_lit{.value = bl->value};

            else if (auto *sl = std::get_if<ast::str_lit>(&e.data))
                res.data = expr::str_lit{.value = sl->value};

            else if (auto *id = std::get_if<ast::var>(&e.data))
                res.data = expr::var{.id = sema.identifier_bindings.at(&e)};

            else if (auto *u = std::get_if<ast::unary>(&e.data)) {
                expr_id sub = lower_expr(fc, *u->sub);
                res.data = expr::unary{.op = u->op, .sub = sub};
            } else if (auto *b = std::get_if<ast::binary>(&e.data)) {
                expr_id left = lower_expr(fc, *b->left);
                expr_id right = lower_expr(fc, *b->right);
                res.data = expr::binary{.op = b->op, .left = left, .right = right};
            } else if (auto *a = std::get_if<ast::assign>(&e.data)) {
                if (std::holds_alternative<ast::var>(a->target)) {
                    expr_id value = lower_expr(fc, *a->value);
                    res.data = expr::assign{.target = sema.assign_bindings.at(&e), .value = value};
                } else {
                    auto &m = std::get<ast::member>(a->target);
                    expr_id key = lower_expr(fc, *m.key);
                    expr_id value = lower_expr(fc, *a->value);
                    res.data = expr::member_assign{
                        .object = sema.assign_bindings.at(&e),
                        .key = key,
                        .value = value,
                    };
                }
            } else if (auto *c = std::get_if<ast::call>(&e.data)) {
                sema::function_id fid = sema.direct_calls.at(&e);
                std::vector<expr_id> args;
                for (auto &arg: c->args)
                    args.push_back(lower_expr(fc, *arg));
                res.data = expr::call{.target = fid, .args = std::move(args)};
            } else if (auto *m = std::get_if<ast::member>(&e.data)) {
                expr_id object = lower_expr(fc, *m->object);
                expr_id key = lower_expr(fc, *m->key);
                res.data = expr::member{.object = object, .key = key};
            } else if (auto *ol = std::get_if<ast::object_lit>(&e.data)) {
                std::vector<std::pair<std::string_view, expr_id> > props;
                for (auto &[key, val]: ol->props)
                    props.emplace_back(key, lower_expr(fc, *val));
                res.data = expr::object_lit{.props = std::move(props)};
            } else if (auto *al = std::get_if<ast::array_lit>(&e.data)) {
                std::vector<expr_id> elements;
                for (auto &elem: al->elements)
                    elements.push_back(lower_expr(fc, *elem));
                res.data = expr::array_lit{.elements = std::move(elements)};
            } else
                assert(false && "unimplemented ast::expr kind in lower_expr");

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
            stmt res{};

            if (auto *b = std::get_if<ast::block>(&s.data)) {
                std::vector<stmt_id> out;
                for (auto &stmt: b->stmts)
                    if (auto id = lower_stmt_opt(fc, *stmt, mod))
                        out.push_back(*id);

                return make_block(fc, std::move(out));
            }
            if (auto *vd = std::get_if<ast::var_declaration>(&s.data)) {
                std::vector<stmt_id> out;
                for (auto &dec: vd->declarators) {
                    std::optional<expr_id> value;
                    if (dec.init)
                        value = lower_expr(fc, dec.init.value());

                    sema::binding_id bid = sema.declarator_bindings.at(&dec);
                    stmt::let_stmt ls{.typ = type::jsvalue, .target = bid, .value = value};
                    out.push_back(append_stmt(fc, stmt{.data = std::move(ls)}));
                }

                return out.size() == 1 ? out[0] : make_block(fc, std::move(out));
            }
            if (auto *r = std::get_if<ast::ret>(&s.data)) {
                std::optional<expr_id> val;
                if (r->value)
                    val = lower_expr(fc, *r->value);
                return append_stmt(fc, stmt{.data = stmt::ret_stmt{val}});
            }
            if (auto *i = std::get_if<ast::if_stmt>(&s.data)) {
                expr_id cond = lower_expr(fc, i->cond);
                stmt_id then_b = lower_stmt(fc, *i->then_branch, mod);
                std::optional<stmt_id> else_b;
                if (i->else_branch)
                    else_b = lower_stmt(fc, *i->else_branch, mod);
                return append_stmt(fc, stmt{.data = stmt::if_stmt{cond, then_b, else_b}});
            }
            if (auto *w = std::get_if<ast::while_stmt>(&s.data)) {
                expr_id cond = lower_expr(fc, w->cond);
                stmt_id body = lower_stmt(fc, *w->body, mod);
                return append_stmt(fc, stmt{.data = stmt::loop_stmt{cond, body}});
            }
            if (auto *dw = std::get_if<ast::do_while_stmt>(&s.data)) {
                // do{body}while(cond) === body; while(cond){body} -- lower the
                // body twice (source AST, not HIR, so this is a plain re-walk,
                // not aliasing) rather than giving loop_stmt a second shape for
                // post-condition loops.
                stmt_id first_body = lower_stmt(fc, *dw->body, mod);
                expr_id cond = lower_expr(fc, dw->cond);
                stmt_id loop_body = lower_stmt(fc, *dw->body, mod);
                stmt_id loop =
                        append_stmt(fc, stmt{.data = stmt::loop_stmt{cond, loop_body}});
                return make_block(fc, {first_body, loop});
            }
            if (auto *fl = std::get_if<ast::for_stmt>(&s.data)) {
                std::vector<stmt_id> outer;
                if (fl->init)
                    outer.push_back(lower_stmt(fc, *fl->init, mod));

                expr_id cond = fl->cond ? lower_expr(fc, *fl->cond) : always_true(fc);

                std::vector<stmt_id> body_stmts;
                body_stmts.push_back(lower_stmt(fc, *fl->body, mod));
                if (fl->update)
                    body_stmts.push_back(append_stmt(
                        fc, stmt{.data = stmt::expr_stmt{lower_expr(fc, *fl->update)}}));

                stmt_id loop_body = make_block(fc, std::move(body_stmts));
                outer.push_back(
                    append_stmt(fc, stmt{.data = stmt::loop_stmt{cond, loop_body}}));
                return make_block(fc, std::move(outer));
            }
            if (auto *es = std::get_if<ast::expr_stmt>(&s.data)) {
                expr_id e = lower_expr(fc, es->value);
                return append_stmt(fc, stmt{.data = stmt::expr_stmt{e}});
            }
            if (std::get_if<ast::brk>(&s.data))
                return append_stmt(fc, stmt{.data = stmt::brk{}});
            if (std::get_if<ast::cont>(&s.data))
                return append_stmt(fc, stmt{.data = stmt::cont{}});

            error("unhandled statement kind in lowering");
            return stmt_id{.id = 69};
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
