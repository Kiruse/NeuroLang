////////////////////////////////////////////////////////////////////////////////
// Definition of the Neuro::Runtime::GC::ManagedMemoryPointer class.
// -----
// Copyright (c) Kiruse 2018
// License: GPL 3.0
#include "GC/NeuroGC.hpp"
#include "GC/ManagedMemoryDescriptor.hpp"
#include "GC/ManagedMemoryPointer.hpp"

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
            GC::markForScan(*this);
        }
    }
}
