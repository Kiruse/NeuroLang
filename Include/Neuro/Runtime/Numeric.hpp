////////////////////////////////////////////////////////////////////////////////
// Standardizes some numeric types and constants.
// ---
// Copyright (c) Kiruse. See license in LICENSE.txt, or online at http://carbon.kirusifix.com/license.
#pragma once

#include <stdint.h>

namespace Neuro {
    typedef int_fast8_t int8;
    typedef uint_fast8_t uint8;
    typedef int_fast16_t int16;
    typedef uint_fast16_t uint16;
    typedef int_fast32_t int32;
    typedef uint_fast32_t uint32;
    typedef int_fast64_t int64;
    typedef uint_fast64_t uint64;
    
#ifdef NEURO_LARGE_HASH
    typedef uint64 hashT;
#else
    typedef uint32 hashT;
#endif
    
    static constexpr uint32 npos = -1;
}
