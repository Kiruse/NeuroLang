////////////////////////////////////////////////////////////////////////////////
// Provides Linux-specific platform implementations.
// -----
// Copyright (c) Kiruse 2018 Germany
#pragma once

#include "NeuroString.hpp"
#include "PlatformConstants.hpp"

namespace Neuro {
    namespace Testing
    {
        int getTtyCols();
        int getTtyRows();
        
        //uint32 launch(const std::path& path, uint32 timeout = (uint32)-1, String& stdout, String& stderr, bool& timedout, uint32& extraerror);
        
        namespace fs
        {
            bool isExecutable(const std::filesystem::path& path);
        }
    }
}
