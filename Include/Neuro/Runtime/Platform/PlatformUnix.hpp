////////////////////////////////////////////////////////////////////////////////
// Unix platform specific constants, functions and classes. Further
// specializations for Linux and Apple can be found in their respective
// modules.
// -----
// Copyright (c) Kiruse 2018 Germany
// License: GNU GPL 3.0
#pragma once

namespace Neuro {
    namespace Platform {
        namespace Unix
        {
            inline bool isWindows() { return false; }
            inline bool isUnix()    { return true; }
            inline bool isLinux()   { return false; }
            inline bool isApple()   { return false; }
            
            extern constexpr char* PathSeparator = ":";
        }
    }
}
