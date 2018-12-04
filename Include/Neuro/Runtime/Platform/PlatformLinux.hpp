////////////////////////////////////////////////////////////////////////////////
// Linux specific constants, functions and classes.
// Further specializations for the various flavors of Linux may be found in their
// own respectively named module files.
// -----
// Copyright (c) Kiruse 2018 Germany
// License: GNU GPL 3.0
#pragma once

#include "PlatformUnix.hpp"

namespace Neuro {
    namespace Platform {
        namespace Linux
        {
            using namespace Unix;
            
            inline bool isLinux() { return true; }
        }
    }
}
