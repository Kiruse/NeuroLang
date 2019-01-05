////////////////////////////////////////////////////////////////////////////////
// Generic platform "dependent" constants, functions and classes as fallback.
// Can be used as template for new platform specializations.
// -----
// Copyright (c) Kiruse 2018 Germany
// License: GNU GPL 3.0
#pragma once

#include "PlatformUnix.hpp"

namespace Neuro {
    namespace Platform {
        namespace Generic
        {
            inline bool isWindows() { return false; }
            inline bool isUnix()    { return false; }
            inline bool isLinux()   { return false; }
            inline bool isApple()   { return false; }
            
            constexpr char* PathSeparator = ":";
        }
    }
}
