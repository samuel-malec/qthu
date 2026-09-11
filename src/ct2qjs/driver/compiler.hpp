#pragma once

#include "config.hpp"
#include "../../asm/asmbuilder.hpp"
#include "../../common/file.hpp"
#include "../codegen.hpp"
#include "../reader.hpp"
#include "../ir.hpp"

namespace qthu::ct2qjs {
    namespace fs = std::filesystem;

    struct compiler {
        diag parse_source(const fs::path &path, symtab &st) {
            std::string content = read_file(path.string());
            auto ptr = std::make_shared<source_file>(path.string(), std::move(content));
            reader r{ptr, st};
            return r.parse();
        }

        void throw_on_diag(const diag &err) {
            if (!err)
                return;

            brq::string_builder b;
            err->print(b);
            throw std::runtime_error(std::string(b.data()));
        }

        void run(config &conf) {
            symtab st;
            auto input = fs::path(conf.in_path);
            auto prelude = fs::path(conf.path_to_cthu) / "prelude.ct";
            auto builtins = fs::path(conf.path_to_cthu) / "builtins.ct";

            throw_on_diag(parse_source(prelude, st));
            throw_on_diag(parse_source(builtins, st));
            throw_on_diag(parse_source(input, st));

            program prog{st};
            prog.lower_to_ir();
            as::asmbuilder builder{};
            codegen cg{prog, builder};
            bc::program bc_prog = cg.lower_to_bc();
            bc_prog.write_binary(conf.out_path);
            std::cout << "qjs bytecode written to: " << conf.out_path << "\n";
        }
    };
}
