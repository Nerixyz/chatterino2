#pragma once

#include <concepts>
#include <iostream>

namespace triegen {

[[noreturn]] void panic(auto msg)
{
    std::cerr << "Fatal: " << msg << '\n';
    exit(EXIT_FAILURE);
}

[[noreturn]] void panic(std::invocable<std::ostream &> auto msg)
{
    std::cerr << "Fatal: ";
    msg(std::cerr);
    std::cerr << '\n';
    exit(EXIT_FAILURE);
}

}  // namespace triegen
