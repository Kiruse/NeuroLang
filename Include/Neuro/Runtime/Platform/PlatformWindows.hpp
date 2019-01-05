////////////////////////////////////////////////////////////////////////////////
// Windows specific constants, functions and classes.
// -----
// Copyright (c) Kiruse 2018 Germany
// License: GNU GPL 3.0
#pragma once

#include <Windows.h>

namespace Neuro {
    namespace Platform {
        namespace Windows
        {
            inline bool isWindows() { return true; }
            inline bool isUnix()    { return false; }
            inline bool isLinux()   { return false; }
            inline bool isApple()   { return false; }
            
            extern constexpr char* PathSeparator = ";";
        }
    }
}
