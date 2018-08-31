////////////////////////////////////////////////////////////////////////////////
// The more elaborate C++ interface and the include used directly in the
// Runtime.cpp source file.
#pragma once

#include "DLLDecl.h"
#include "Error.hpp"
#include "NeuroBuffer.hpp"
#include "NeuroString.hpp"

namespace Neuro {
    namespace Runtime
    {
        /**
         * @brief Gets the Error object representing the last occurred error.
         * 
         * @return Copy of the last occurred error.
         * @see Neuro::NoError
         */
        NEURO_API Error getLastError();
        
        /**
         * @brief Initialize Neuro's runtime.
         */
        NEURO_API Error init();
        
        /**
         * @brief Shutdown Neuro's runtime.
         */
        NEURO_API Error shutdown();
    }
}
