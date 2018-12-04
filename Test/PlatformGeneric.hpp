////////////////////////////////////////////////////////////////////////////////
// Generic platform-independent, yet defaulted fallback implementations.
// -----
// Copyright (c) Kiruse 2018 Germany
#pragma once

#include "PlatformConstants.hpp"

namespace Neuro {
    namespace Test {
        namespace Platform {
            namespace Generic
            {
                inline int getTtyCols() { return 80; }
                inline int getTtyRows() { return 20; }
                
                namespace fs
                {
                    inline bool isExecutable(const std::filesystem::path& path) {
                        return false;
                    }
                }
            }
        }
    }
}
