#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../../common/visit.hpp"
#include "../frontend/ast.hpp"

namespace qthu::js2ct::sema {
    template<typename... Args>
    void error(Args &&... args) {
        std::ostringstream out;
        ( out << ... << std::forward<Args>(args) );
        throw std::runtime_error(out.str());
    }

    template<typename... Args>
    void error(const location &where, Args &&... args) {
        std::ostringstream out;
        out << where << ": ";
        ( out << ... << std::forward<Args>(args) );
        throw std::runtime_error(out.str());
    }

    struct name_id {
        uint32_t value;

        auto operator<=>(const name_id &) const = default;
    };

    struct symbol_id {
        uint32_t value;
    };

    struct binding_id {
        uint32_t value;
    };

    struct function_id {
        uint32_t value;
    };

    struct scope_id {
        uint32_t value;
    };

    struct symbol {
        symbol_id id;

        enum class kind_t {
            variable,
            function,
        } kind;

        name_id name;
        scope_id declared_in;

        std::optional<binding_id> binding;
        std::optional<function_id> function;
    };

    struct binding {
        binding_id id;
        ast::var_declaration::kind_t kind;
        scope_id declared_in;
        bool initialized = false;
    };

    struct function {
        function_id id;
        scope_id scope;
        size_t arity;
    };

    struct scope {
        enum class kind {
            global,
            function,
            loop,
            block,
        };

        scope_id id;
        kind category;

        std::optional<scope_id> parent;
        std::optional<function_id> enclosing_function;
        std::map<name_id, symbol_id> declarations;

        void add(name_id nid, symbol_id sid, std::vector<std::string> &names) {
            if (declarations.contains(nid))
                error("Duplicate declaration of ", names[nid.value]);

            declarations[nid] = sid;
        }
    };

    struct analysis_result {
        std::vector<std::string> names;
        std::vector<symbol> declarations;
        std::vector<binding> bindings;
        std::vector<function> functions;
        std::vector<scope> scopes;

        function_id global_function;
        std::unordered_map<ast::stmt *, scope_id> stmt_scopes;
        std::unordered_map<ast::var_declarator *, binding_id> declarator_bindings;
        std::unordered_map<ast::param *, binding_id> param_bindings;
        std::unordered_map<ast::expr *, binding_id> identifier_bindings;
        std::unordered_map<ast::expr *, binding_id> assign_bindings;
        std::unordered_map<ast::expr *, function_id> direct_calls;
        std::unordered_map<ast::stmt *, function_id> stmt_functions;

        std::string function_name(function_id fid) const {
            for (auto &sym: declarations)
                if (sym.kind == symbol::kind_t::function && sym.function && sym.function->value == fid.value)
                    return names.at(sym.name.value);
            return "<script>"; // the implicit top-level function has no declaring symbol
        }
    };

    struct analyzer {
        analysis_result result;
        std::map<std::string, name_id, std::less<> > interned_names;

        bool is_in_loop(scope_id current) {
            std::optional<scope_id> curr_scope_id = current;
            while (curr_scope_id) {
                auto &curr_scope = get_scope(curr_scope_id.value());
                if (curr_scope.category == scope::kind::loop)
                    return true;
                curr_scope_id = curr_scope.parent;
            }

            return false;
        }

        name_id intern(std::string_view name) {
            if (auto it = interned_names.find(name); it != interned_names.end())
                return it->second;

            name_id id{.value = static_cast<uint32_t>(result.names.size())};
            std::string sname = std::string(name);
            interned_names[sname] = id;
            result.names.push_back(sname);
            return id;
        }

        // NOTE: when creating function scope, the enclosing function needs to be set manually,
        // because it needs to be created before the function itself
        scope_id declare_scope(scope::kind k, std::optional<scope_id> parent) {
            scope_id id{static_cast<uint32_t>(result.scopes.size())};
            scope s{.id = id, .category = k, .parent = parent};
            if (parent)
                s.enclosing_function = get_scope(parent.value()).enclosing_function;

            result.scopes.push_back(s);
            return id;
        }

        binding_id add_binding(ast::var_declaration::kind_t k, scope_id declared_in, bool initialized) {
            binding_id bid{static_cast<uint32_t>(result.bindings.size())};
            binding b{
                .id = bid,
                .kind = k,
                .declared_in = declared_in,
                .initialized = initialized,
            };

            result.bindings.push_back(b);
            return bid;
        }

        symbol_id add_symbol(symbol::kind_t k, name_id name, scope_id declared_in,
                             std::optional<binding_id> binding,
                             std::optional<function_id> function) {
            symbol_id sid{.value = static_cast<uint32_t>(result.declarations.size())};
            symbol sym{
                .id = sid,
                .kind = k,
                .name = name,
                .declared_in = declared_in,
                .binding = binding,
                .function = function,
            };

            result.declarations.push_back(sym);
            return sid;
        }

        symbol &get_symbol(symbol_id id) {
            return result.declarations.at(id.value);
        }

        binding &get_binding(binding_id id) {
            return result.bindings.at(id.value);
        }

        function &get_function(function_id id) {
            return result.functions.at(id.value);
        }

        scope &get_scope(scope_id id) {
            return result.scopes.at(id.value);
        }

        void declare_var(ast::var_declaration &vd, scope_id curr_scope) {
            for (auto &declarator: vd.declarators) {
                name_id nid = intern(declarator.name);
                binding_id bid = add_binding(vd.kind, curr_scope, declarator.init.has_value());
                result.declarator_bindings[&declarator] = bid;
                symbol_id sid = add_symbol(symbol::kind_t::variable, nid, curr_scope, bid, {});
                get_scope(curr_scope).add(nid, sid, result.names);
            }
        }

        binding_id add_param_binding(ast::param &p, scope_id curr_scope) {
            name_id nid = intern(p.name);
            binding_id bid = add_binding(ast::var_declaration::kind_t::let, curr_scope, true);
            result.param_bindings[&p] = bid;
            symbol_id sid = add_symbol(symbol::kind_t::variable, nid, curr_scope, bid, {});
            get_scope(curr_scope).add(nid, sid, result.names);
            return bid;
        }

        std::optional<symbol_id> lookup(scope_id current, std::string_view name) {
            if (!interned_names.contains(name))
                return {};

            name_id nid = intern(name);
            std::optional<scope_id> curr_id = current;

            while (curr_id) {
                auto &curr_scope = get_scope(curr_id.value());
                auto it = curr_scope.declarations.find(nid);
                if (it != curr_scope.declarations.end())
                    return it->second;
                curr_id = curr_scope.parent;
            }

            return {};
        }

        void declare_block(ast::block &b, scope_id curr_scope) {
            for (auto &stmt: b.stmts)
                declare_stmt(*stmt, curr_scope);
        }

        void declare_stmt(ast::stmt &s, scope_id curr_scope) {
            std::visit(overloaded{
                           [ & ](ast::block &b) {
                               scope_id new_scope = declare_scope(scope::kind::block, curr_scope);
                               get_scope(new_scope).enclosing_function = get_scope(curr_scope).enclosing_function;
                               result.stmt_scopes[&s] = new_scope;
                               declare_block(b, new_scope);
                           },
                           [ & ](ast::var_declaration &vd) {
                               declare_var(vd, curr_scope);
                           },
                           [ & ](ast::fn_declaration &fd) {
                               // "main" is reserved: the top-level script itself always
                               // compiles to `structure main` (linear2cthu.hpp's lowerer,
                               // matching cthuc's hardcoded find_main_id() lookup for
                               // `structure main :: run`) -- a JS function also named `main`
                               // would collide with it (two `structure main` blocks in the
                               // emitted .ct, which cthuc's reader rejects as "main already
                               // defined", a confusing error that doesn't point at the real
                               // cause). Reject it here instead, with a clear explanation.
                               if (fd.name == "main")
                                   error(s.loc, "'main' is a reserved function name -- the top-level script itself "
                                         "compiles to the structure named 'main', so a function can't be named "
                                         "that too. Please rename this function.");

                               scope_id fn_scope = declare_scope(scope::kind::function, curr_scope);
                               function_id fid{.value = static_cast<uint32_t>(result.functions.size())};
                               result.stmt_scopes[&s] = fn_scope;
                               result.stmt_functions[&s] = fid;

                               function f{.id = fid, .scope = fn_scope, .arity = fd.params.size()};
                               result.functions.push_back(f);

                               get_scope(fn_scope).enclosing_function = fid;

                               name_id nid = intern(fd.name);
                               symbol_id sid = add_symbol(symbol::kind_t::function, nid, curr_scope, {}, fid);
                               get_scope(curr_scope).add(nid, sid, result.names);

                               for (auto &param: fd.params)
                                   add_param_binding(param, fn_scope);

                               declare_block(fd.body, fn_scope);
                           },
                           [ & ](ast::ret &) {
                               // Nothing to declare -- a return statement can't
                               // introduce a new scope, function, or binding.
                           },
                           [ & ](ast::if_stmt &i) {
                               declare_stmt(*i.then_branch, curr_scope);
                               if (i.else_branch)
                                   declare_stmt(*i.else_branch, curr_scope);
                           },
                           [ & ](ast::do_while_stmt &dw) {
                               scope_id loop_scope = declare_scope(scope::kind::loop, curr_scope);
                               result.stmt_scopes[&s] = loop_scope;
                               declare_stmt(*dw.body, loop_scope);
                           },
                           [ & ](ast::while_stmt &w) {
                               scope_id loop_scope = declare_scope(scope::kind::loop, curr_scope);
                               result.stmt_scopes[&s] = loop_scope;
                               declare_stmt(*w.body, loop_scope);
                           },
                           [ & ](ast::for_stmt &fl) {
                               scope_id loop_scope = declare_scope(scope::kind::loop, curr_scope);
                               result.stmt_scopes[&s] = loop_scope;
                               if (fl.init)
                                   declare_stmt(*fl.init, loop_scope);
                               declare_stmt(*fl.body, loop_scope);
                           },
                           [ & ](ast::expr_stmt &) {
                               // Nothing to declare -- JS doesn't allow a
                               // function/var declaration in expression position.
                           },
                           [ & ](ast::brk &) {
                               // Nothing to declare.
                           },
                           [ & ](ast::cont &) {
                               // Nothing to declare.
                           },
                       }, s.data);
        }

        void resolve_expr(ast::expr &e, scope_id curr_scope) {
            std::visit(overloaded{
                           [ & ](ast::int_lit &) {
                               // Nothing to resolve -- no identifiers in a literal.
                           },
                           [ & ](ast::bool_lit &) {
                               // Nothing to resolve.
                           },
                           [ & ](ast::str_lit &) {
                               // Nothing to resolve.
                           },
                           [ & ](ast::var &id) {
                               auto sid = lookup(curr_scope, id.name);
                               if (!sid)
                                   error(e.loc, "undeclared identified '", id.name, "'");

                               result.identifier_bindings[&e] = get_symbol(sid.value()).binding.value();
                           },
                           [ & ](ast::unary &u) {
                               resolve_expr(*u.sub, curr_scope);
                           },
                           [ & ](ast::binary &b) {
                               resolve_expr(*b.left, curr_scope);
                               resolve_expr(*b.right, curr_scope);
                           },
                           [ & ](ast::assign &a) {
                               resolve_expr(*a.value, curr_scope);

                               if (auto *v = std::get_if<ast::var>(&a.target)) {
                                   auto sid = lookup(curr_scope, v->name);
                                   if (!sid)
                                       error(e.loc, "unknown identifier '", v->name, "'");

                                   auto &sym = get_symbol(sid.value());
                                   if (sym.kind != symbol::kind_t::variable)
                                       error(e.loc, "invalid target of an assignment");

                                   result.assign_bindings[&e] = sym.binding.value();

                                   auto &bi = get_binding(sym.binding.value());
                                   if (bi.kind == ast::var_declaration::kind_t::constant)
                                       error(e.loc, "can't assign to a const-qualified variable");
                               } else {
                                   auto &m = std::get<ast::member>(a.target);
                                   // Single-level only (the parser already enforces this): the
                                   // member's own object must be a plain identifier, not
                                   // another member expression.
                                   auto &obj_var = std::get<ast::var>(m.object->data);

                                   auto sid = lookup(curr_scope, obj_var.name);
                                   if (!sid)
                                       error(e.loc, "unknown identifier '", obj_var.name, "'");

                                   auto &sym = get_symbol(sid.value());
                                   if (sym.kind != symbol::kind_t::variable)
                                       error(e.loc, "invalid target of an assignment");

                                   // Not const-checked: JS allows `const o = {}; o.x = 1;` --
                                   // const only forbids rebinding `o` itself, not mutating
                                   // what it points to.
                                   result.assign_bindings[&e] = sym.binding.value();

                                   resolve_expr(*m.key, curr_scope);
                               }
                           },
                           [ & ](ast::call &c) {
                               ast::expr *callee_ptr = c.callee.get();
                               if (!std::holds_alternative<ast::var>(callee_ptr->data))
                                   error(e.loc, "expected an identifier");

                               auto &calle_name = std::get<ast::var>(callee_ptr->data);

                               // assert(...) is a magic, never-declared compiler builtin
                               // (ast2hir.hpp recognizes it syntactically in statement
                               // position and lowers it to hir::stmt::assert_stmt instead
                               // of a real call) -- skip the "must be a declared function"
                               // check for this one name; just resolve its argument.
                               if (calle_name.name == "assert") {
                                   if (c.args.size() != 1)
                                       error(e.loc, "assert() takes exactly one argument");
                                   resolve_expr(*c.args[0], curr_scope);
                                   return;
                               }

                               auto sid = lookup(curr_scope, calle_name.name);
                               if (!sid)
                                   error(e.loc, "undecared identifier");

                               auto &sym = get_symbol(sid.value());
                               if (sym.kind != symbol::kind_t::function)
                                   error(e.loc, "expected a function");

                               function_id fid = sym.function.value();
                               result.direct_calls[&e] = fid;
                               auto &f = get_function(fid);

                               // TODO: javascript allows calling functions with less arguments than described by the
                               // function signature, we will have to work out way through it
                               if (c.args.size() != f.arity)
                                   error(e.loc, "call arity doesn't match");

                               for (auto &arg: c.args)
                                   resolve_expr(*arg, curr_scope);
                           },
                           [ & ](ast::member &m) {
                               resolve_expr(*m.object, curr_scope);
                               resolve_expr(*m.key, curr_scope);
                           },
                           [ & ](ast::object_lit &ol) {
                               for (auto &[key, val]: ol.props)
                                   resolve_expr(*val, curr_scope);
                           },
                           [ & ](ast::array_lit &al) {
                               for (auto &elem: al.elements)
                                   resolve_expr(*elem, curr_scope);
                           },
                       }, e.data);
        }

        void resolve_block(ast::block &b, scope_id block_scope) {
            for (auto &sub: b.stmts)
                resolve_stmt(*sub, block_scope);
        }

        void resolve_stmt(ast::stmt &s, scope_id curr_scope) {
            std::visit(overloaded{
                           [ & ](ast::block &b) {
                               scope_id &block_scope = result.stmt_scopes.at(&s);
                               resolve_block(b, block_scope);
                           },
                           [ & ](ast::var_declaration &vd) {
                               for (auto &decl: vd.declarators) {
                                   auto sid = lookup(curr_scope, decl.name);
                                   assert(sid);

                                   auto &sym = get_symbol(sid.value());
                                   auto &bi = get_binding(sym.binding.value());
                                   if (bi.kind == ast::var_declaration::kind_t::constant && !decl.init)
                                       error(s.loc, "const declaration requires an initializer");

                                   if (decl.init.has_value()) {
                                       resolve_expr(*decl.init, curr_scope);
                                       bi.initialized = true;
                                       get_binding(*sym.binding).initialized = true;
                                   }
                               }
                           },
                           [ & ](ast::fn_declaration &fd) {
                               scope_id &fn_scope = result.stmt_scopes.at(&s);
                               resolve_block(fd.body, fn_scope);
                           },
                           [ & ](ast::ret &r) {
                               if (!get_scope(curr_scope).enclosing_function)
                                   error(s.loc, "return statement outside of a function");
                               if (r.value)
                                   resolve_expr(*r.value, curr_scope);
                           },
                           [ & ](ast::if_stmt &i) {
                               resolve_expr(i.cond, curr_scope);
                               resolve_stmt(*i.then_branch, curr_scope);
                               if (i.else_branch)
                                   resolve_stmt(*i.else_branch, curr_scope);
                           },
                           [ & ](ast::do_while_stmt &dw) {
                               scope_id &loop_scope = result.stmt_scopes.at(&s);
                               resolve_expr(dw.cond, loop_scope);
                               resolve_stmt(*dw.body, loop_scope);
                           },
                           [ & ](ast::while_stmt &w) {
                               scope_id &loop_scope = result.stmt_scopes.at(&s);
                               resolve_expr(w.cond, loop_scope);
                               resolve_stmt(*w.body, loop_scope);
                           },
                           [ & ](ast::for_stmt &fl) {
                               scope_id &loop_scope = result.stmt_scopes.at(&s);
                               if (fl.init)
                                   resolve_stmt(*fl.init, loop_scope);
                               if (fl.cond)
                                   resolve_expr(*fl.cond, loop_scope);
                               if (fl.update)
                                   resolve_expr(*fl.update, loop_scope);
                               resolve_stmt(*fl.body, loop_scope);
                           },
                           [ & ](ast::expr_stmt &e) {
                               resolve_expr(e.value, curr_scope);
                           },
                           [ & ](ast::brk &) {
                               if (!is_in_loop(curr_scope))
                                   error(s.loc, "'break' statement not within a loop");
                           },
                           [ & ](ast::cont &) {
                               if (!is_in_loop(curr_scope))
                                   error(s.loc, "'continue' statement not within a statemnt");
                           },
                       }, s.data);
        }

        analysis_result run(ast::program &ast) {
            scope_id global = declare_scope(scope::kind::global, {});
            function_id gfid{.value = static_cast<uint32_t>(result.functions.size())};
            function f{.id = gfid, .scope = global, .arity = 0};

            result.functions.push_back(f);
            result.global_function = gfid;

            for (auto &stmt: ast.statements)
                declare_stmt(*stmt, global);

            for (auto &stmt: ast.statements)
                resolve_stmt(*stmt, global);

            return result;
        }
    };
} // namespace qthu::js2ct::sema
