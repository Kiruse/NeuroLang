////////////////////////////////////////////////////////////////////////////////
// Provides windows-specific platform implementations.
// -----
// Copyright (c) Kiruse 2018 Germany
#pragma once

#include <windows.h>

#include "NeuroString.hpp"
#include "PlatformConstants.hpp"

namespace Neuro {
    namespace Test {
        namespace Platform {
            namespace Windows
            {
                int getTtyCols();
                int getTtyRows();
                
                uint32 launch(const std::path& path, uint32 timeout, String& stdout, String& stderr, bool& timedout, uint32& extraerror);
                
                namespace fs
                {
                    inline bool isExecutable(const std::filesystem::path& path) {
                        return path.extension() == ".exe";
                    }
                }
            }
        }
    }
}
