////////////////////////////////////////////////////////////////////////////////
// The lowest possible interface of our Garbage Collector. Usually you'll rather
// avoid working with these functions as calling them directly and improperly
// has high chances of breaking your code.
// 
// Various other indirect interfaces tie into the Garbage Collector and allow for
// a significantly more convenient and safer use.
// 
// The Low Level Interface allows us to implement and embed custom types into the
// Garbage Collector, but this requires at least a custom scanning algorithm.
// The GC traces arbitrary managed memory through pointers on known data structures,
// not through any special pointer format. As an example, the objects have a very
// specific memory layout which allows the GC to determine where pointers are
// and what they point to. Any other approach would require us to describe a
// certain data structure, or consult the object in question directly asking it
// to check if it contains a given pointer, which is slow and outside our control.
// This approach allows us to drastically optimize the performance of the scanners.
// -----
// Copyright (c) Kiruse 2018
// License: GPL 3.0
#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>

#include "Delegate.hpp"
#include "MaybeAnError.hpp"
#include "Misc.hpp"

#include "NeuroSet.hpp"

#include "GC/ManagedMemoryPointer.hpp"

namespace Neuro {
    namespace Runtime {
        /**
         * The GC resembles a static class of C#. It does not permit instantiation
         * in any way and all of its methods are static. This helps us granting
         * all its components friend access to data structures without having to
         * keep long, explicit lists.
         */
        class GC
        {
            using ScannerDelegate = Delegate<void, StandardHashMap<ManagedMemoryPointerBase>&>;
            
            /**
             * Initializes the Garbage Collector.
             * 
             * During its initialization, the Garbage Collector launches a new
             * thread to manage memory concurrently.
             */
            static Error init();
            
            /**
             * Gracefully cleans up the Garbage Collector and all its associated
             * managed memory. May be a heavy operation!
             */
            static Error destroy();
            
            /**
             * Allocate `size` bytes of managed trivial memory.
             * 
             * Trivial memory differs from non-trivial memory in that move and
             * destruction operations can be performed in bulk.
             */
            static ManagedMemoryPointerBase allocateTrivial(uint32 size);
            
            template<typename T>
            static void copyNonTrivialType(void* target, const void* source) {
                new (reinterpret_cast<T*>(target)) T(*reinterpret_cast<T*>(source));
            }
            
            template<typename T>
            static void destroyNonTrivialType(void* target) {
                reinterpret_cast<T*>(target)->~T();
            }
            
            /**
             * Allocate `size` bytes of managed non-trivial memory.
             * 
             * When the GC moves or destroys the allocated non-trivial memory,
             * it consults the provided delegates.
             * 
             * Non-trivial memory differs from trivial memory in that its members
             * require per-member copy and destruction operations.
             */
            static ManagedMemoryPointerBase allocateNonTrivial(uint32 size, const Delegate<void, void*, const void*>& copyDelegate, const Delegate<void, void*>& destroyDelegate);
            
            /**
             * Allocate managed memory for a specific data type, optionally
             * specifying whether it should be treated as trivial data even if
             * it's not. The latter might be useful if the data type may not be
             * trivial, but trivially copyable, e.g. creates only flat copies.
             * Neuro's Objects are one such example.
             */
            template<typename T>
            static ManagedMemoryPointer<T> allocate(bool trivial = std::is_trivial_v<T>) {
                if (trivial) {
                    return ManagedMemoryPointer<T>(allocateTrivial(sizeof(T)));
                }
                return ManagedMemoryPointer<T>(allocateNonTrivial(sizeof(T), Delegate<void, void*, const void*>::FunctionDelegate<copyNonTrivialType<T>>(), Delegate<void, void*>::FunctionDelegate<destroyNonTrivialType<T>>()));
            }
            
            /**
             * Marks the given managed memory pointer for scanning in the next
             * scan phase.
             */
            static void markForScan(ManagedMemoryPointerBase pointer);
            
            /**
             * Registers a new scanner to call during the scan phase. It is
             * expected that the scanner removes found pointers from the delegate's
             * passed in argument.
             * 
             * The scanner for our Object type is one such scanner.
             */
            static Error registerMemoryScanner(const Delegate<void, StandardHashSet<ManagedMemoryPointerBase>&>& scanner);
        };
    }
}