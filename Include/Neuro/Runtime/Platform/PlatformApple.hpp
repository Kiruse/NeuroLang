////////////////////////////////////////////////////////////////////////////////
// Apple platform specific constants, functions and classes.
// 
// Apple currently has no priority for this project. This module exists as a
// placeholder. It's likely the project will only start supporting apple once
// it has progressed far enough to be able to compile its compiler itself, which
// likely means a complete rewrite of the compiler and interpreter to begin with.
// -----
// Copyright (c) Kiruse 2018 Germany
// License: GNU GPL 3.0
#pragma once

#include "PlatformUnix.hpp"

namespace Neuro {
    namespace Platform {
        namespace Apple
        {
            using namespace Unix;
            
            inline bool isApple() { return true; }
        }
    }
}
