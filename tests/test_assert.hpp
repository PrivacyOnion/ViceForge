#pragma once

#include <cstdlib>
#include <iostream>

#define VF_CHECK(expr)                                                                     \
    do {                                                                                   \
        if (!(expr)) {                                                                     \
            std::cerr << "Check failed at " << __FILE__ << ":" << __LINE__                 \
                      << " -> " << #expr << "\n";                                          \
            std::abort();                                                                  \
        }                                                                                  \
    } while (false)
