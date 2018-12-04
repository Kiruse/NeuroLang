////////////////////////////////////////////////////////////////////////////////
// Include-only broker determining which platform-dependent code to use.
// -----
// Copyright (c) Kiruse 2018 Germany
// License: GNU GPL 3.0
#pragma once

#ifdef _WIN32
# include "PlatformWindows.hpp"
#elif __linux__
# include "PlatformLinux.hpp"
#elif __APPLE__
// See: #include <TargetConditionals.h> to distinguish between OSX and iOS.
# include "PlatformApple.hpp"
#else
# include "PlatformGeneric.hpp"
#endif

namespace Neuro {
    namespace Platform
    {
#ifdef _WIN32
        using namespace Windows;
#elif __linux__
        using namespace Linux;
#elif __APPLE__
        using namespace Apple;
#else
        using namespace Generic;
#endif
    }
}
