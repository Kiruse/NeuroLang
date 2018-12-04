////////////////////////////////////////////////////////////////////////////////
// Low level descriptors of managed memory sections as well as the table
// administering them.
// 
// Descriptors themselves are POD types that hold some meta data on specific
// associated managed data.
// 
// ManagedMemoryPointers store indices in the table. When the GC moves managed
// data around, it updates the descriptor appropriately. The indices are never
// changed. When data is released, the index is invalidated, the respective
// event triggered, and the index may be overridden.
// -----
// Copyright (c) Kiruse 2018
// License: GPL 3.0
#pragma once

#include <atomic>
#include <memory>
#include <shared_mutex>

#include "Allocator.hpp"
#include "Delegate.hpp"
#include "Maybe.hpp"
#include "Numeric.hpp"

namespace Neuro {
    namespace Runtime {
        struct ManagedMemoryDescriptor {
            /**
             * Whether the data stored in the associated memory section is
             * intended to be trivial. If so, move_deleg and destroy_deleg
             * are ignored, otherwise they will be consulted accordingly.
             */
            uint32 trivial : 1;
            
            /** 
             * Whether this associated memory section is marked as garbage
             * and will be swept soon.
             */
            uint32 garbage : 1;
            
            /**
             * Actual address where the managed data lies.
             */
            std::atomic<void*> addr;
            
            /**
             * UID of the associated address. This is used to inform unmanaged
             * weak pointers to managed data that the data has been cleared and
             * the pointer is invalid, i.e. returns nullptr upon indirection.
             */
            uint32 uid;
            
            /**
             * Number of bytes in this associated section, including the
             * overhead.
             */
            uint32 size;
            
            /**
             * Operation to conduct when moving non-trivial data around.
             */
            Maybe<Delegate<void, void*, const void*>> copy_deleg;
            
            /**
             * Operation to conduct when destroying non-trivial data.
             */
            Maybe<Delegate<void, void*>> destroy_deleg;
            
            
            ManagedMemoryDescriptor() : trivial(false), garbage(false), addr(nullptr), uid(0), size(0), copy_deleg(), destroy_deleg() {}
            ManagedMemoryDescriptor(const ManagedMemoryDescriptor&) = delete;
            ManagedMemoryDescriptor(ManagedMemoryDescriptor&&) = delete;
            ManagedMemoryDescriptor& operator=(const ManagedMemoryDescriptor&) = delete;
            ManagedMemoryDescriptor& operator=(ManagedMemoryDescriptor&&) = delete;
            ~ManagedMemoryDescriptor() = default;
        };
        
        /**
         * Container class to hold and administer ManagedMemoryDescriptors
         * thread-safely.
         */
        class ManagedMemoryTable {
            std::atomic_flag growing;
            std::atomic<uint32> hardLockRequests;
            std::shared_mutex bufferMutex;
            std::condition_variable_any bufferNotify;
            uint8* buffer;
            uint32 chunks;
            uint32 chunkSize;
            static std::atomic<uint32> nextUid;
            
        public: // RAII
            ManagedMemoryTable();
            ManagedMemoryTable(const ManagedMemoryTable&) = delete;
            ManagedMemoryTable(ManagedMemoryTable&&) = delete;
            ManagedMemoryTable& operator=(const ManagedMemoryTable&) = delete;
            ManagedMemoryTable& operator=(ManagedMemoryTable&&) = delete;
            ~ManagedMemoryTable() = default;
            
        public:
            uint32 addTrivial(void* addr, uint32 size);
            uint32 addNonTrivial(void* addr, uint32 size, Delegate<void, void*, const void*>& copyDeleg, Delegate<void, void*>& destroyDeleg);
            void remove(void* addr);
            
            ManagedMemoryDescriptor& get(uint32 index) { return *reinterpret_cast<ManagedMemoryDescriptor*>(buffer + calcOffset(index)); }
            const ManagedMemoryDescriptor& get(uint32 index) const { return *reinterpret_cast<ManagedMemoryDescriptor*>(buffer + calcOffset(index)); }
            ManagedMemoryDescriptor& operator[](uint32 index) { return get(index); }
            const ManagedMemoryDescriptor& operator[](uint32 index) const { return get(index); }
            
            uint32 size() const { return chunks * chunkSize; }
            
        private:
            uint32 claimUnusedIndex(void* addr);
            void grow();
            
            void alloc(uint32 chunks);
            void realloc(uint32 chunks);
            void dealloc();
            
            static void initChunk(uint8* buffer, uint32 chunkIndex, uint32 chunkSize);
            static void copyChunk(uint8* source, uint8* buffer, uint32 chunkIndex, uint32 chunkSize);
            
            uint32 calcOffset(uint32 index) const;
            static uint32 calcBytes(uint32 chunks, uint32 chunkSize);
            
        public: // Events
            /**
             * Triggered when managed data has been released. Delegates receive
             * the data's associated UID.
             */
            EventDelegate<uint32> onRelease;
        };
    }
}
