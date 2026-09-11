#pragma once

#include "ostream"
#include <stdexcept>

template <typename... Args>
void error(Args&&... args) {
    std::ostringstream out;
    (out << ... << std::forward<Args>(args));
    throw std::runtime_error(out.str());
}
