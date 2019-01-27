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
            
            
            ManagedMemoryOverhead() = default;
            ManagedMemoryOverhead(uint32 elementSize, uint32 count = 1) : elementSize(elementSize), count(count), garbageState(EGarbageState::Live) {}
            
            /**
             * Gets the number of bytes in the subsequent memory buffer.
             */
            uint32 getBufferBytes() const {
                return elementSize * count;
            }
            
            /**
             * Gets the number of bytes that this represented memory region
             * including the overhead itself and its subsequent buffer occupy.
             */
            uint32 getTotalBytes() const {
                return sizeof(ManagedMemoryOverhead) + getBufferBytes();
            }
            
            /**
             * Gets a pointer to the buffer start address for convenience.
             */
            void* getBufferPointer() const {
                return const_cast<void*>(reinterpret_cast<const void*>(this + 1));
            }
            
            /**
             * Gets a pointer to the address immediately after the memory buffer.
             */
            void* getBeyondPointer() const {
                return (uint8*)getBufferPointer() + elementSize * count;
            }
        };
    }
}
