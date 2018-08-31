////////////////////////////////////////////////////////////////////////////////
// Implementation of the Neuromancy Runtime Library.
// 
// IMPORTANT: The NeuroRT is split in two library interfaces: the C and C++
// interfaces! The C++ interface is designed for more convenient access to the
// library when you're absolutely certain you won't need cross-compiler compatibility
// because all versions of your program will ship with their own compiled runtime.
// 
// For the uninitiated, cross-compiler here even refers to different versions of
// your compiler! E.g. VS2015 will generate other binary files than VS2017 and
// hence a C++ .DLL could not be shared between programs compiled with different
// compiler versions.
// ---
// Copyright (c) Kiruse. See license in LICENSE.txt, or online at http://neuro.kirusifix.com/license.
#include "Runtime.h"
#include "Runtime.hpp"
#include "Error.hpp"
#include "NeuroBuffer.hpp"
#include "NeuroString.hpp"

////////////////////////////////////////////////////////////////////////////////
// Globals

static const Neuro::Error g_lastError = Neuro::NoError::instance();


////////////////////////////////////////////////////////////////////////////////
// C Interface implementation

extern "C" {
    int neuroInit() {
        return Neuro::Runtime::init().code();
    }
    
    int neuroShutdown() {
        return Neuro::Runtime::shutdown().code();
    }
    
    const char* neuroGetLastErrorMessage() {
        return g_lastError.message().c_str();
    }
}


////////////////////////////////////////////////////////////////////////////////
// C++ Interface and the actual implementation

namespace Neuro {
    namespace Runtime
    {
        Error getLastError() {
            return g_lastError;
        }
        
        Error init() {
            
            return NoError::instance();
        }
        
        Error shutdown() {
            
            return NoError::instance();
        }
    }
}
