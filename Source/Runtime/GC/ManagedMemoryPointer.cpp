////////////////////////////////////////////////////////////////////////////////
// Definition of the Neuro::Runtime::GC::ManagedMemoryPointer class.
// -----
// Copyright (c) Kiruse 2018
// License: GPL 3.0
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
        
        void* ManagedMemoryPointerBase::get() const {
            return GCGlobals::dataTable.get(*this);
        }
        
        void ManagedMemoryPointerBase::clear() {
            
        }
        
        ManagedMemoryOverhead* ManagedMemoryPointerBase::getHeadPointer() const {
            return reinterpret_cast<ManagedMemoryOverhead*>(reinterpret_cast<uint8*>(get()) - sizeof(ManagedMemoryOverhead));
        }
    }
}
