////////////////////////////////////////////////////////////////////////////////
// Provides windows-specific platform implementations.
// -----
// Copyright (c) Kiruse 2018 Germany
#pragma once

#include <windows.h>

namespace Neuro {
    namespace Testing
    {
        int getTtyCols() {
            CONSOLE_SCREEN_BUFFER_INFO csbi;
            GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
            return csbi.srWindow.Right - csbi.srWindow.Left + 1;
        }
        
        int getTtyRows() {
            CONSOLE_SCREEN_BUFFER_INFO csbi;
            GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
            return csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
        }
    }
}
