////////////////////////////////////////////////////////////////////////////////
// Overhead to a continuous managed memory buffer. Holds some meta data on the
// associated buffer, including size and management flags.
// -----
// Copyright (c) Kiruse 2018 Germany
// License: GNU GPL-3.0
#pragma once

#include "Delegate.hpp"
#include "Maybe.hpp"
#include "Numeric.hpp"

namespace Neuro {
    namespace Runtime
    {
        namespace EGarbageState {
            enum {
                Live,
                Marked,
                Dying,
                Swept,
            };
        }
        
        struct ManagedMemoryOverhead
        {
            /**
             * Size of one element in this associated continuous managed memory.
             */
            uint32 elementSize;
            
            /**
             * Number of elements in this associated continuous managed memory.
             */
            uint32 count;
            
            /**
             * Garbage Collection state this associated managed memory is in.
             */
            uint32 garbageState : 2;
            
            /**
             * Optional copy delegate. The GC ignores this when it assumes it
             * operates on trivial memory.
             */
            Maybe<Delegate<void, void*, const void*>> copyDelegate;
            
            /**
             * Optional destruction delegate. The GC ignores this when it
             * assumes it operates on trivial memory.
             */
            Maybe<Delegate<void, void*>> destroyDelegate;
            
            
            static void init(ManagedMemoryOverhead* inst, uint32 elementSize, uint32 count = 1) {
                inst->elementSize = elementSize;
                inst->count = count;
                inst->garbageState = EGarbageState::Live;
            }
        };
    }
}
