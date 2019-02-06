////////////////////////////////////////////////////////////////////////////////
// Definition of the Neuro::Runtime::GC::ManagedMemoryPointer class.
// -----
// Copyright (c) Kiruse 2018
// License: GPL 3.0
#include <cassert>

#include "GC/NeuroGC.hpp"
#include "GC/ManagedMemoryOverhead.hpp"
#include "GC/ManagedMemoryPointer.hpp"
#include "GC/ManagedMemoryTable.hpp"

#define OVERHEAD_CAST(ptr) reinterpret_cast<ManagedMemoryOverhead*>(ptr)

namespace Neuro {
    namespace Runtime {
        namespace GCGlobals {
            extern ManagedMemoryTable dataTable;
        }
        
        ManagedMemoryTable* managedMemoryPointer_UseTable = &GCGlobals::dataTable;
        
        void* ManagedMemoryPointerBase::get(uint32 index) const {
            auto* head = getHeadPointer();
            if (!head) return nullptr;
			return (uint8*)(head->getBufferPointer()) + head->elementSize * index;
        }
        
        ManagedMemoryOverhead* ManagedMemoryPointerBase::getHeadPointer() const {
            void* buffer = managedMemoryPointer_UseTable->get(*this);
            if (!buffer) return nullptr;
            return reinterpret_cast<ManagedMemoryOverhead*>(reinterpret_cast<uint8*>(buffer) - sizeof(ManagedMemoryOverhead));
        }
        
        void ManagedMemoryPointerBase::useOverheadLookupTable(ManagedMemoryTable* table) {
            managedMemoryPointer_UseTable = table;
        }
    }
}
