////////////////////////////////////////////////////////////////////////////////
// Base for platform-specific code.
// -----
// Copyright (c) Kiruse 2018 Germany
#pragma once

#ifdef _WIN32
# include "PlatformWindows.hpp"
#elif __linux__
# include "PlatformLinux.hpp"
//#elif __APPLE__
// Sarry, no apple code yet :c
#else
# include "PlatformGeneric.hpp"
#endif
