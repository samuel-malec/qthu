#pragma once

#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <variant>
#include <vector>

#include "../sema/types.hpp"

namespace qthu::js2ct::lin {
    struct value {
        uint32_t id;
    };

    inline bool operator<(const value &lhs, const value &rhs) {
        return lhs.id < rhs.id;
    }

    using constant = std::variant<uint64_t, bool>;
    using argument = std::variant<constant, value>;

    struct instr;

    struct cons_data {
        constant c;
        value target;
    };

    // A string literal is deliberately its own instr kind, not folded into
    // cons_data/constant: constant feeds into `argument`, which every binary/
    // unary/call operand accepts via an implicit conversion -- adding
    // std::string to that variant made std::string-valued `in`/`out` name
    // lists (used throughout linear2cthu.hpp's emit() calls) ambiguously
    // constructible as either vector<std::string> or vector<argument>, since
    // a plain string now implicitly converts to argument through constant.
    struct str_cons_data {
        std::string str;
        value target;
    };

    struct get_data {
        argument obj;
        argument key;
        value target;
    };

    struct cons_obj_data {
        value target;
    };

    struct cons_arr_data {
        value target;
    };

    // obj[key] = val, producing the (linear) updated object -- the field is
    // named `val`, not `value`, so it doesn't shadow the `value` type name
    // needed for `target`'s own declaration right below it.
    struct set_data {
        argument obj;
        argument key;
        argument val;
        value target;
    };

    struct unary_data {
        op_kind op;
        argument arg1;
        value target;
    };

    struct binary_data {
        op_kind op;
        argument arg1;
        argument arg2;
        value target;
    };

    struct copy_data {
        argument arg1;
        value target;
    };

    struct dup_data {
        argument arg1;
        value first;
        value second;
    };

    // A call can only ever propagate one output (same constraint loop_data
    // works around -- see pack_values/unpack_values in linear2cthu.hpp), so
    // each branch's live bindings travel packed: then_outputs/else_outputs
    // are what then_body/else_body leave each live binding holding (same
    // order as params; else_outputs == params unchanged when there's no
    // explicit else branch), and outputs are fresh ids for the merged
    // post-if value of each binding -- the enclosing scope gets reassigned
    // to these, same order as params, so code after the if sees the right
    // values regardless of which branch ran.
    //
    // Exception: exhaustively_returns (both branches' lowered bodies end in
    // a real `return`) means nothing after the if in this block can ever
    // execute -- there's nothing to thread forward, so then_outputs/
    // else_outputs/outputs are left empty and unused. codegen keeps the
    // simple single-"out" shape in that case (each branch's own trailing
    // return already produces "out" locally; the dispatch's own result
    // becomes the enclosing function's return value one level up, tail-
    // position style) -- packing it anyway would silently break that,
    // since the enclosing function would stop looking like it produces
    // "out" at all. Detected as a direct check (does the lowered body's
    // last instruction happen to be a return), not a general "does this
    // statement always terminate" analysis -- doesn't see through further
    // nesting (e.g. a branch whose entire content is itself another
    // exhaustively-returning if isn't recognized), a known, documented gap.
    struct if_data {
        argument cond;
        std::vector<instr> then_body;
        std::vector<instr> else_body;
        std::vector<value> params;
        bool exhaustively_returns = false;
        std::vector<value> then_outputs;
        std::vector<value> else_outputs;
        std::vector<value> outputs;
    };

    struct loop_data {
        std::vector<instr> cond_body; // re-run on *every* entry to the loop (initial call and every
        // recursive re-entry alike), since cond depends on live state
        argument cond; // the value cond_body produces
        std::vector<value> dispatch_args; // same bindings' values *after* cond_body ran (whatever
        // survived its dups) -- used for the post-cond dispatch
        // call, since `params` itself may be partly consumed by
        // then. Same order as params.
        std::vector<instr> body; // the loop's own body, run once per continuing iteration
        std::vector<value> params; // live values at loop entry (also each generated function's
        // own "in" parameter names -- body/cond_body are lowered
        // independently, each starting fresh from these)
        std::vector<value> next_params; // same bindings, post-body values, same order as params
        std::vector<value> outputs; // fresh ids for each binding's post-loop value; the
        // enclosing scope is reassigned to these, same order as params
    };

    struct drop_data {
        value target;
    };

    struct call_data {
        sema::function_id callee;
        std::vector<argument> args;
        value target;
    };

    struct ret_data {
        std::optional<argument> arg;
    };

    struct brk_data {
    };

    struct cont_data {
    };

    // No target: consumes its argument, produces nothing (matches
    // qjs_val_assert's own signature).
    struct assert_data {
        argument arg;
    };

    struct instr {
        using data_type = std::variant<
            cons_data,
            str_cons_data,
            unary_data,
            binary_data,
            copy_data,
            dup_data,
            drop_data,
            if_data,
            loop_data,
            call_data,
            get_data,
            cons_obj_data,
            cons_arr_data,
            set_data,
            ret_data,
            assert_data,
            brk_data,
            cont_data>;
        data_type data;

        void for_each_use(auto &&f) {
            auto visit_operand = [ & ](argument &o) {
                if (auto *v = std::get_if<value>(&o))
                    f(*v);
            };

            std::visit([ & ](auto &&d) {
                using T = std::decay_t<decltype( d )>;
                if constexpr (std::is_same_v<T, unary_data>)
                    visit_operand(d.arg1);
                else if constexpr (std::is_same_v<T, binary_data>) {
                    visit_operand(d.arg1);
                    visit_operand(d.arg2);
                } else if constexpr (std::is_same_v<T, copy_data>)
                    visit_operand(d.arg1);
                else if constexpr (std::is_same_v<T, if_data>)
                    visit_operand(d.cond);
                else if constexpr (std::is_same_v<T, call_data>)
                    for (auto &a: d.args)
                        visit_operand(a);
                else if constexpr (std::is_same_v<T, get_data>) {
                    visit_operand(d.obj);
                    visit_operand(d.key);
                } else if constexpr (std::is_same_v<T, set_data>) {
                    visit_operand(d.obj);
                    visit_operand(d.key);
                    visit_operand(d.val);
                } else if constexpr (std::is_same_v<T, ret_data>) {
                    if (d.arg)
                        visit_operand(*d.arg);
                } else if constexpr (std::is_same_v<T, assert_data>)
                    visit_operand(d.arg);
            }, data);
        }

        void set_target(value new_target) {
            std::visit([ & ](auto &&d) {
                using T = std::decay_t<decltype( d )>;
                if constexpr (requires { d.target; })
                    d.target = new_target;
            }, data);
        }

        std::optional<value> get_target() {
            return std::visit([](auto &&d) -> std::optional<value> {
                using T = std::decay_t<decltype( d )>;
                if constexpr (requires { d.target; })
                    return d.target;
                else
                    return std::nullopt;
            }, data);
        }
    };

    struct function {
        sema::function_id name;
        std::vector<value> params;
        std::vector<instr> body;
    };

    struct program {
        std::vector<function> functions;

        function &get_script() {
            assert(!functions.empty());
            return functions[0];
        }
    };
}
