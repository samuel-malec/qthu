#pragma once

#include <cstring>
#include <iostream>
#include <string>
#include <stdexcept>

#include "../../common/error.hpp"

namespace qthu::ct2qjs {
    struct config {
        std::string in_path;
        std::string out_path;
        std::string path_to_cthu;
    };

    inline void help() {
        std::cout << "Usage:\n"
                << "./ct2qjs file.ct\n"
                << "-p path (path to folder containing prelude.ct and builtins.ct)\n"
                << "[-o out]\n";
    }

    inline config parse_config(int argc, char *const*argv) {
        if (argc < 1)
            throw std::runtime_error("Usage: ./ct2qjs file.js [-o out]\n");

        if (strcmp(argv[0], "-h") == 0) {
            help();
            exit(0);
        }

        std::string file_in = argv[0];
        std::string file_out = "a.qbc";
        std::string path_to_cthu = "/";

        for (int i = 1; i < argc; ++i) {
            if (strcmp(argv[i], "-o") == 0) {
                if (i >= argc - 1)
                    throw std::runtime_error("missing argument of -o");

                ++i;
                file_out = argv[i];
            } else if (strcmp(argv[i], "-p") == 0) {
                if (i >= argc - 1)
                    throw std::runtime_error("missing argument of -p");

                ++i;
                path_to_cthu = argv[i];
            } else
                error("invalid ct2qjs flag: ", argv[i]);
        }

        return {
            .in_path = file_in,
            .out_path = file_out,
            .path_to_cthu = path_to_cthu
        };
    }
}
