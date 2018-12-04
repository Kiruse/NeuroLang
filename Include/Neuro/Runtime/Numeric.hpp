////////////////////////////////////////////////////////////////////////////////
// Standardizes some numeric types and constants.
// ---
// Copyright (c) Kiruse. See license in LICENSE.txt, or online at http://carbon.kirusifix.com/license.
#pragma once

#include <cstdint>

namespace Neuro {
    typedef std::int8_t int8;
    typedef std::uint8_t uint8;
    typedef std::int16_t int16;
    typedef std::uint16_t uint16;
    typedef std::int32_t int32;
    typedef std::uint32_t uint32;
    typedef std::int64_t int64;
    typedef std::uint64_t uint64;
    
    typedef std::int_fast8_t fast_int8;
    typedef std::uint_fast8_t fast_uint8;
    typedef std::int_fast16_t fast_int16;
    typedef std::uint_fast16_t fast_uint16;
    typedef std::int_fast32_t fast_int32;
    typedef std::uint_fast32_t fast_uint32;
    typedef std::int_fast64_t fast_int64;
    typedef std::uint_fast64_t fast_uint64;
    
#ifdef NEURO_LARGE_HASH
    typedef uint64 hashT;
#else
    typedef uint32 hashT;
#endif
    
    static constexpr uint32 npos = -1;
}
