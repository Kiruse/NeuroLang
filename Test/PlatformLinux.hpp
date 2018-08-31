////////////////////////////////////////////////////////////////////////////////
// Provides Linux-specific platform implementations.
// -----
// Copyright (c) Kiruse 2018 Germany
#pragma once

#include <sys/ioctl.h>
#include <unistd.h>

namespace Neuro {
    namespace Testing
    {
        int getTtyCols() {
            winsize w;
            ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
            return w.ws_col;
        }
        
        int getTtyRows() {
            winsize w;
            ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
            return w.ws_row;
        }
    }
}
