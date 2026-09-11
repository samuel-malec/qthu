#pragma once

#include <queue>

#include "../../common/error.hpp"
#include "../../common/visit.hpp"
#include "../ir/linear.hpp"
#include "../ir/hir.hpp"

namespace qthu::js2ct::lin {
    struct rename_env {
        std::unordered_map<std::uint32_t, value> scope;

        value &at(sema::binding_id bid) {
            if (auto it = scope.find(bid.value); it != scope.end())
                return it->second;

            assert(false && "value not found");
        }

        void declare(sema::binding_id bid, value v) {
            scope[bid.value] = v;
        }

        void reassign(sema::binding_id bid, value v) {
            if (auto it = scope.find(bid.value); it != scope.end()) {
                it->second = v;
                return;
            }

            assert(false && "reassigning a binding that was never declared");
        }

        bool has(sema::binding_id bid) const {
            return scope.contains(bid.value);
        }

        void forget(sema::binding_id bid) {
            scope.erase(bid.value);
        }
    };

    struct value_namer {
        uint32_t next = 0;

        value fresh() {
            return lin::value{.id = next++};
        };
    };

    struct hir_to_linear {
        hir::function &fn;
        sema::analysis_result &sema;

        uint32_t next_synth_binding = static_cast<uint32_t>(sema.bindings.size());
        value_namer vn;

        sema::binding_id fresh_binding() {
            return sema::binding_id{next_synth_binding++};
        }

        argument lower_expr(std::vector<instr> &sink, rename_env &env, hir::expr_id eid) {
            const auto &node = fn.get(eid);

            return std::visit(overloaded{
                                  [ & ](const hir::expr::int_lit &lit) -> argument {
                                      value target = vn.fresh();
                                      sink.push_back(instr{cons_data{.c = lit.value, .target = target}});
                                      return target;
                                  },
                                  [ & ](const hir::expr::bool_lit &lit) -> argument {
                                      value target = vn.fresh();
                                      sink.push_back(instr{cons_data{.c = lit.value, .target = target}});
                                      return target;
                                  },
                                  [ & ](const hir::expr::str_lit &lit) -> argument {
                                      value target = vn.fresh();
                                      sink.push_back(instr{
                                          str_cons_data{.str = std::string(lit.value), .target = target}
                                      });
                                      return target;
                                  },
                                  [ & ](const hir::expr::var &v) -> argument {
                                      value fst = vn.fresh();
                                      value snd = vn.fresh();
                                      sink.push_back(
                                          instr{dup_data{.arg1 = env.at(v.id), .first = fst, .second = snd}});
                                      env.reassign(v.id, snd);
                                      return fst;
                                  },
                                  [ & ](const hir::expr::unary &u) -> argument {
                                      argument sub = lower_expr(sink, env, u.sub);
                                      value target = vn.fresh();
                                      sink.push_back(instr{unary_data{u.op, sub, target}});
                                      return target;
                                  },
                                  [ & ](const hir::expr::binary &b) -> argument {
                                      if (b.op == AND || b.op == OR)
                                          return lower_short_circuit(sink, env, b.op, b.left, b.right);

                                      argument l = lower_expr(sink, env, b.left);
                                      argument r = lower_expr(sink, env, b.right);
                                      value target = vn.fresh();
                                      sink.push_back(instr{binary_data{b.op, l, r, target}});
                                      return target;
                                  },
                                  [ & ](const hir::expr::assign &a) -> argument {
                                      argument val = lower_expr(sink, env, a.value);
                                      auto target = env.at(a.target);
                                      sink.push_back(instr{drop_data{.target = target}});
                                      sink.push_back(instr{copy_data{.arg1 = val, .target = target}});

                                      value fst = vn.fresh();
                                      value snd = vn.fresh();
                                      sink.push_back(instr{dup_data{.arg1 = target, .first = fst, .second = snd}});
                                      env.reassign(a.target, snd);
                                      return fst;
                                  },
                                  [ & ](const hir::expr::call &c) -> argument {
                                      std::vector<argument> args;

                                      for (auto arg: c.args)
                                          args.push_back(lower_expr(sink, env, arg));

                                      value target = vn.fresh();
                                      sink.push_back(instr{call_data{c.target, std::move(args), target}});
                                      return target;
                                  },
                                  [ & ](const hir::expr::member &m) -> argument {
                                      argument obj = lower_expr(sink, env, m.object);
                                      argument key = lower_expr(sink, env, m.key);
                                      value target = vn.fresh();
                                      sink.push_back(instr{get_data{obj, key, target}});
                                      return target;
                                  },
                                  [ & ](const hir::expr::object_lit &ol) -> argument {
                                      value obj = vn.fresh();
                                      sink.push_back(instr{cons_obj_data{obj}});

                                      for (auto &[key, val_id]: ol.props) {
                                          value key_val = vn.fresh();
                                          sink.push_back(instr{str_cons_data{std::string(key), key_val}});

                                          argument val = lower_expr(sink, env, val_id);

                                          value next_obj = vn.fresh();
                                          sink.push_back(instr{set_data{obj, key_val, val, next_obj}});
                                          obj = next_obj;
                                      }

                                      return obj;
                                  },
                                  [ & ](const hir::expr::array_lit &al) -> argument {
                                      value arr = vn.fresh();
                                      sink.push_back(instr{cons_arr_data{arr}});

                                      for (size_t idx = 0; idx < al.elements.size(); ++idx) {
                                          value key_val = vn.fresh();
                                          sink.push_back(instr{
                                              cons_data{.c = static_cast<uint64_t>(idx), .target = key_val}
                                          });

                                          argument val = lower_expr(sink, env, al.elements[idx]);

                                          value next_arr = vn.fresh();
                                          sink.push_back(instr{set_data{arr, key_val, val, next_arr}});
                                          arr = next_arr;
                                      }

                                      return arr;
                                  },
                                  [ & ](const hir::expr::member_assign &ma) -> argument {
                                      argument key = lower_expr(sink, env, ma.key);
                                      argument val = lower_expr(sink, env, ma.value);
                                      value val1 = vn.fresh();
                                      value val2 = vn.fresh();
                                      sink.push_back(instr{dup_data{.arg1 = val, .first = val1, .second = val2}});

                                      argument obj = env.at(ma.object);

                                      value new_obj = vn.fresh();
                                      sink.push_back(instr{set_data{obj, key, val1, new_obj}});
                                      env.reassign(ma.object, new_obj);

                                      return val2;
                                  },
                              }, node.data);
        }

        // a && b  ==  let tmp = a; if (truthy(tmp)) { tmp = b; }  tmp
        // a || b  ==  let tmp = a; if (truthy(tmp)) { /* keep */ } else { tmp = b; }  tmp
        argument lower_short_circuit(std::vector<instr> &sink, rename_env &env, op_kind op,
                                     hir::expr_id left_id, hir::expr_id right_id) {
            argument left_val = lower_expr(sink, env, left_id);

            value cond_copy = vn.fresh();
            value tmp_copy = vn.fresh();
            sink.push_back(instr{dup_data{.arg1 = left_val, .first = cond_copy, .second = tmp_copy}});

            sema::binding_id tmp_bid = fresh_binding();
            env.declare(tmp_bid, tmp_copy);

            std::vector<sema::binding_id> live_bindings{};
            std::vector<value> params{};
            for (auto &[bid, val]: env.scope) {
                live_bindings.push_back(sema::binding_id{bid});
                params.push_back(val);
            }

            auto assign_tmp_to_rhs = [ & ](std::vector<instr> &body, rename_env &branch_env) {
                argument rhs_val = lower_expr(body, branch_env, right_id);
                value target = branch_env.at(tmp_bid);
                body.push_back(instr{drop_data{.target = target}});
                body.push_back(instr{copy_data{.arg1 = rhs_val, .target = target}});
                branch_env.reassign(tmp_bid, target);
            };

            std::vector<instr> then_body;
            rename_env then_env = env;
            std::vector<instr> else_body;
            rename_env else_env = env;

            if (op == AND)
                assign_tmp_to_rhs(then_body, then_env);
            else
                assign_tmp_to_rhs(else_body, else_env);

            std::vector<value> then_outputs{};
            for (auto &bid: live_bindings)
                then_outputs.push_back(then_env.at(bid));

            std::vector<value> else_outputs{};
            for (auto &bid: live_bindings)
                else_outputs.push_back(else_env.at(bid));

            std::vector<value> outputs{};
            for (auto &bid: live_bindings) {
                value out = vn.fresh();
                outputs.push_back(out);
                env.reassign(bid, out);
            }

            sink.push_back(instr{
                if_data{
                    .cond = cond_copy,
                    .then_body = std::move(then_body),
                    .else_body = std::move(else_body),
                    .params = std::move(params),
                    .then_outputs = std::move(then_outputs),
                    .else_outputs = std::move(else_outputs),
                    .outputs = std::move(outputs),
                }
            });

            value result = env.at(tmp_bid);
            env.forget(tmp_bid);
            return result;
        }

        //  This doesn't work for the stacks that we return from the procedure
        void cleanup_env(rename_env &env, std::vector<lin::instr> &sink) {
            for (auto &[bid, val]: env.scope)
                sink.push_back(lin::instr{.data = lin::drop_data{.target = val}});
        }

        void lower_stmt(std::vector<lin::instr> &sink, rename_env &env, hir::stmt_id sid) {
            const auto &node = fn.get(sid);
            std::visit(overloaded{
                           [ & ](const hir::stmt::expr_stmt &es) {
                               lower_expr(sink, env, es.expr);
                           },
                           [ & ](const hir::stmt::block &b) {
                               for (auto sub: b.stmts)
                                   lower_stmt(sink, env, sub);
                           },
                           [ & ](const hir::stmt::let_stmt &ls) {
                               value v;
                               if (ls.value) {
                                   argument a = lower_expr(sink, env, *ls.value);
                                   if (auto *value = std::get_if<lin::value>(&a))
                                       v = *value;
                                   else {
                                       v = vn.fresh();
                                       sink.push_back(instr{copy_data{a, v}});
                                   }
                               } else
                                   v = vn.fresh();
                               env.declare(ls.target, v);
                           },
                           [ & ](const hir::stmt::ret_stmt &rs) {
                               std::optional<argument> val;
                               if (rs.value)
                                   val = lower_expr(sink, env, *rs.value);
                               sink.push_back(instr{ret_data{val}});
                           },
                           [ & ](const hir::stmt::if_stmt &ifd) {
                               auto condarg = lower_expr(sink, env, ifd.cond);

                               std::vector<sema::binding_id> live_bindings{};
                               std::vector<value> params{};
                               for (auto &[bid, val]: env.scope) {
                                   live_bindings.push_back(sema::binding_id{bid});
                                   params.push_back(val);
                               }

                               std::vector<instr> then_body;
                               rename_env then_env = env;
                               lower_stmt(then_body, then_env, ifd.then_branch);
                               bool then_returns = !then_body.empty() && std::holds_alternative<ret_data>(
                                                       then_body.back().data);

                               std::vector<instr> else_body;
                               rename_env else_env = env;
                               bool else_returns = false;
                               if (ifd.else_branch) {
                                   lower_stmt(else_body, else_env, ifd.else_branch.value());
                                   else_returns = !else_body.empty() && std::holds_alternative<ret_data>(
                                                      else_body.back().data);
                               }

                               // Both branches definitely return: nothing after the if in
                               // this block can ever execute, so there's nothing to thread
                               // forward -- keep the simple single-"out" shape (see the
                               // exhaustively_returns comment on if_data's own definition).
                               bool exhaustively_returns = then_returns && ifd.else_branch && else_returns;

                               if (exhaustively_returns) {
                                   cleanup_env(then_env, then_body);
                                   cleanup_env(else_env, else_body);

                                   sink.push_back(instr{
                                       if_data{
                                           .cond = condarg,
                                           .then_body = std::move(then_body),
                                           .else_body = std::move(else_body),
                                           .params = std::move(params),
                                           .exhaustively_returns = true,
                                       }
                                   });
                               } else {
                                   std::vector<value> then_outputs{};
                                   for (auto &bid: live_bindings)
                                       then_outputs.push_back(then_env.at(bid));

                                   // else_env starts as a copy of env either way; if there's
                                   // no explicit else branch it's never mutated, so this
                                   // naturally yields "live bindings pass through unchanged".
                                   std::vector<value> else_outputs{};
                                   for (auto &bid: live_bindings)
                                       else_outputs.push_back(else_env.at(bid));

                                   std::vector<value> outputs{};
                                   for (auto &bid: live_bindings) {
                                       value out = vn.fresh();
                                       outputs.push_back(out);
                                       env.reassign(bid, out);
                                   }

                                   sink.push_back(instr{
                                       if_data{
                                           .cond = condarg,
                                           .then_body = std::move(then_body),
                                           .else_body = std::move(else_body),
                                           .params = std::move(params),
                                           .then_outputs = std::move(then_outputs),
                                           .else_outputs = std::move(else_outputs),
                                           .outputs = std::move(outputs),
                                       }
                                   });
                               }
                           },
                           [ & ](const hir::stmt::loop_stmt &st) {
                               std::vector<sema::binding_id> live_bindings{};
                               std::vector<value> params{};
                               for (auto &[bid, val]: env.scope) {
                                   live_bindings.push_back(sema::binding_id{bid});
                                   params.push_back(val);
                               }

                               rename_env cond_env = env;
                               std::vector<instr> cond_body;
                               argument condarg = lower_expr(cond_body, cond_env, st.cond);


                               std::vector<value> dispatch_args{};
                               for (auto &bid: live_bindings)
                                   dispatch_args.push_back(cond_env.at(bid));

                               rename_env body_env = env;
                               std::vector<instr> body;
                               lower_stmt(body, body_env, st.body);

                               std::vector<value> next_params{};
                               for (auto &bid: live_bindings)
                                   next_params.push_back(body_env.at(bid));

                               std::vector<value> outputs{};
                               for (auto &bid: live_bindings) {
                                   value out = vn.fresh();
                                   outputs.push_back(out);
                                   env.reassign(bid, out);
                               }

                               sink.push_back(instr{
                                   loop_data{
                                       .cond_body = std::move(cond_body),
                                       .cond = condarg,
                                       .dispatch_args = std::move(dispatch_args),
                                       .body = std::move(body),
                                       .params = std::move(params),
                                       .next_params = std::move(next_params),
                                       .outputs = std::move(outputs)
                                   }
                               });
                           },
                           [ & ](const hir::stmt::assert_stmt &as) {
                               argument arg = lower_expr(sink, env, as.arg);
                               sink.push_back(instr{assert_data{arg}});
                           },
                           [ & ](const hir::stmt::brk &) {
                               error("'break' is not supported yet");
                           },
                           [ & ](const hir::stmt::cont &) {
                               error("'continue' is not supported yet");
                           },
                       }, node.data);
        }

        function lower_function() {
            function res{.name = fn.id, .body{}};
            rename_env env{};

            for (auto &p: fn.parameters) {
                value v = vn.fresh();
                env.declare(p, v);
                res.params.push_back(v);
            }
            lower_stmt(res.body, env, fn.body_root);
            cleanup_env(env, res.body);

            return res;
        }
    };

    struct lowerer {
        sema::analysis_result &sema;

        program lower(hir::module &mod) {
            program prog{};
            hir_to_linear script_lowering{mod.script, sema};
            prog.functions.push_back(std::move(script_lowering.lower_function()));

            for (auto &fn: mod.functions) {
                hir_to_linear fl{fn, sema};
                prog.functions.push_back(std::move(fl.lower_function()));
            }
            return prog;
        }
    };
} // namespace qthu::js2ct::lin
