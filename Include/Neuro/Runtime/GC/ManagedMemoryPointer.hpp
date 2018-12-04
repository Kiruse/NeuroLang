////////////////////////////////////////////////////////////////////////////////
// A special pointer designed explicitly for managed objects within the Neuro
// Runtime, including native objects.
// 
// Comes in three flavors: generic base pointer (implemented through a void*),
// strongly typed derived pointers (which reinterpret_cast the void*), and
// specialized Neuro::Runtime::Object pointers. Prefer avoiding use of the first
// where possible.
// 
// These are implemented as part of the low level API and are designed to behave
// almost like regular pointers - with the exception that they can / should
// (HIGHLY RECOMMENDED) be assigned to only through other instances yielded from
// the Neuro::Runtime::GC::allocate function.
// 
// As with all low level API, tampering with the data structures in an unforseen
// way may completely break everything or lead to memory leaks and corruption.
// -----
// Copyright (c) Kiruse 2018
// License: GPL 3.0
#pragma once

#include <atomic>
#include <memory>

#include "Delegate.hpp"
#include "HashCode.hpp"
#include "Numeric.hpp"

namespace Neuro {
    namespace Runtime {
        // Forward-declare Object class, as we need a pointer to it.
        class Object;
        
        class ManagedMemoryPointerBase {
            friend class GC;
            friend class ManagedMemoryTable;
            
            /**
             * Index of the managed memory descriptor within the internal
             * global table.
             */
            uint32 tableIndex;
            
            /**
             * Locally unique ID of the row, designed to minimize clashing
             * potential. This is specifically used to account for weak pointers
             * outside of the managed memory realm to ensure that the pointer
             * they expect is still the same, even if the row has been reclaimed
             * and reused.
             */
            hashT rowuid;
            
        public:
            ManagedMemoryPointerBase() : tableIndex(npos), rowuid(0) {}
            ~ManagedMemoryPointerBase() { clear(); }
            
            void* get() const;
            void clear();
            
            bool operator==(const ManagedMemoryPointerBase& other) const { return tableIndex == other.tableIndex; }
            bool operator!=(const ManagedMemoryPointerBase& other) const { return !(*this == other); }
            
            operator bool() const { return tableIndex != npos && get() != nullptr; }
        };
        
        template<typename T>
        class ManagedMemoryPointer : public ManagedMemoryPointerBase {
        public:
            ManagedMemoryPointer() = default;
            ManagedMemoryPointer(ManagedMemoryPointerBase other) : ManagedMemoryPointerBase(other) {}
            
            T* get() const { return reinterpret_cast<T* const>(ManagedMemoryPointerBase::get()); }
            
            T& operator*() const { return *get(); }
            T* operator->() const { return get(); }
        };
        
        inline hashT calculateHash(const ManagedMemoryPointerBase& pointer) {
            return Neuro::calculateHash(pointer.get());
        }
    }
}
