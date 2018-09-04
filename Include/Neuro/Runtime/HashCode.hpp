////////////////////////////////////////////////////////////////////////////////
// Hash code calculators for various commonly used data types, as well as some
// utility functions to aid in hash composition.
// -----
// Copyright (c) Kiruse 2018
// License: GPL 3.0
#pragma once

#include "NeuroString.hpp"
#include "Numeric.hpp"
#include "Misc.hpp"

namespace Neuro
{
    /**
     * Simple combination of two hash codes using XOR.
     * 
     * Heed this caveat: XOR is commutative, meaning you receive the same hash
     * code if you combine 'a' with 'b', and vice versa!
     * 
     * However, results of this operation consist of 50% set bits, which leads
     * to a relatively even distribution of bits.
     */
    inline hashT combineHashSimple(hashT lhs, hashT rhs) {
        return lhs ^ rhs;
    }
    
    /**
     * Combines two hash codes using bitwise implication.
     * 
     * However, bitwise implication is suboptimal because the results of this
     * operation consist of 75% set bits.
     * It seems easier to find a non-commutative solution when combining three
     * hash codes at once.
     */
    inline hashT combineHashOrdered(hashT lhs, hashT rhs) {
        return bitwiseImplication(lhs, rhs);
    }
    
    template<typename NumericType>
    hashT calculateHash(NumericType value) { return static_cast<hashT>(value); }
    
    /** Used heavily in the Neuro Lang. Object properties are essentially addressed by their hash. */
    inline hashT calculateHash(const String& string) {
        uint32 result = 0;
        for (uint32 i = 0; i < string.length(); ++i) {
            result = combineHashOrdered(result, string[i]);
        }
        return result;
    }
    
    /** Alternative to Neuro::String, which simply wraps the c-string in a temporary Neuro::String and calculates the hashcode thereof. */
    inline hashT calculateHash(const char* string) {
        return calculateHash(String(string));
    }
}
