////////////////////////////////////////////////////////////////////////////////
// Miscellaneous helper functions used across the project, but don't have a
// particular category I can think of...
// -----
// Copyright (c) Kiruse 2018
// License: GPL 3.0
#pragma once

namespace Neuro
{
    /**
     * The `is` namespace provides various utilities to test data, e.g. `is::equal`
     * to test if two values of the same type are equal.
     * It comes with a nested `not` namespace containing the same utilities,
     * except it negates them.
     */
    namespace is
    {
        /**
         * A simple templated utility function used in particular in combination
         * with templated classes or functions to specify the comparison algorithm
         * to use. Resorts to simply using the == operator of the type.
         */
        template<typename T>
        inline bool equal(const T& lhs, const T& rhs) {
            return lhs == rhs;
        }
        
        template<typename T>
        inline bool lessThan(const T& lhs, const T& rhs) {
            return lhs < rhs;
        }
        
        template<typename T>
        inline bool greaterThan(const T& lhs, const T& rhs) {
            return lhs > rhs;
        }
        
        template<typename T>
        inline bool lessThanOrEqual(const T& lhs, const T& rhs) {
            return lhs <= rhs;
        }
        
        template<typename T>
        inline bool greaterThanOrEqual(const T& lhs, const T& rhs) {
            return lhs >= rhs;
        }
        
        namespace not
        {
            template<typename T>
            inline bool equal(const T& lhs, const T& rhs) {
                return lhs != rhs;
            }
            
            template<typename T>
            inline bool lessThan(const T& lhs, const T& rhs) {
                return lhs >= rhs;
            }
            
            template<typename T>
            inline bool greaterThan(const T& lhs, const T& rhs) {
                return lhs <= rhs;
            }
            
            template<typename T>
            inline bool lessThanOrEqual(const T& lhs, const T& rhs) {
                return lhs > rhs;
            }
            
            template<typename T>
            inline bool greaterThanOrEqual(const T& lhs, const T& rhs) {
                return lhs < rhs;
            }
        }
    }
    
    /** Calculates the result of bitwise mathematical implication. */
    inline uint32 bitwiseImplication(uint32 lhs, uint32 rhs) {
        return rhs | ~(lhs ^ rhs);
    }
    
    /**
     * Toggles between one of two types based on a boolean value. Assumes the
     * first data type if `Switch` is false, otherwise the second.
     */
    template<bool Switch, typename T, typename U> struct toggle_type { typedef T type; };
    template<typename T, typename U> struct toggle_type<false, T, U> { typedef U type; };
    template<bool Switch, typename T, typename U> using toggle_type_t = typename toggle_type<Switch, T, U>::type;
    
    template<bool Const, typename T> struct toggle_const { typedef const T type; };
    template<typename T> struct toggle_const<false, T> { typedef T type; };
    template<bool Const, typename T> using toggle_const_t = typename toggle_const<Const, T>::type;
}
