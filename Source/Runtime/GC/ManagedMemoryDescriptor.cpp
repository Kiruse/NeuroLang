////////////////////////////////////////////////////////////////////////////////
// Implementation
#include "Assert.hpp"
#include "GC/ManagedMemoryDescriptor.hpp"

namespace Neuro {
    namespace Runtime
    {
        struct ManagedMemoryTableChunkHead {
            uint32 size;
            std::atomic<uint32> occupied;
            std::atomic<ManagedMemoryTableChunkHead*> next;
            ManagedMemoryDescriptor* first;
        };
        
        
        std::atomic<uint32> ManagedMemoryTable::nextUid(0);
        
        ManagedMemoryTable::ManagedMemoryTable() : hardLockRequests(0), bufferMutex(), buffer(nullptr), chunks(0), chunkSize(100) {
            alloc(10);
            growing.clear();
        }
        
        uint32 ManagedMemoryTable::addTrivial(void* addr, uint32 size) {
            // Enter shared lock, but prioritize exclusive locks.
            std::shared_lock lock(bufferMutex);
            bufferNotify.wait(lock, []{ return hardLockRequests.load() == 0; });
            
            uint32 index = claimUnusedIndex(addr);
            if (index != npos) {
                ManagedMemoryDescriptor& desc = get(index);
                desc.trivial = false;
                desc.garbage = false;
                desc.size = size;
                desc.uid = nextUid.fetch_add(1);
            }
            return index;
        }
        
        uint32 ManagedMemoryTable::addNonTrivial(void* addr, uint32 size, Delegate<void, void*, const void*>& copyDeleg, Delegate<void, void*>& destroyDeleg) {
            // Enter shared lock, but prioritize exclusive locks.
            std::shared_lock lock(bufferMutex);
            bufferNotify.wait(lock, []{ return hardLockRequests.load() == 0; });
            
            uint32 index = claimUnusedIndex(addr);
            if (index != npos) {
                ManagedMemoryDescriptor& desc = get(index);
                desc.trivial = true;
                desc.garbage = false;
                desc.size = size;
                desc.uid = nextUid.fetch_add(1);
                
                copyDeleg.copyTo(&desc.copy_deleg.get());
                destroyDeleg.copyTo(&desc.destroy_deleg.get());
            }
            return index;
        }
        
        void ManagedMemoryTable::remove(uint32 index) {
            // Enter shared lock, but prioritize exclusive locks.
            std::shared_lock lock(bufferMutex);
            bufferNotify.wait(lock, []{ return hardLockRequests.load() == 0; });
            
            ManagedMemoryDescriptor& desc = get(index);
            desc.uid = 0;
            desc.addr = nullptr;
        }
        
        uint32 ManagedMemoryTable::claimUnusedIndex(void* addr) {
            // Enter shared lock
            std::shared_lock lock(bufferMutex);
            
            // but prioritize exclusive locks.
            bufferNotify.wait(lock, []{ return hardLockRequests.load() == 0; });
            
            // Used as failsafe to prevent infinite loops
            static constexpr uint8 maxIterations = 3;
            uint8 iterations = 0;
            
            // For atomic comparison reference
            void* cmp = nullptr;
            do {
                // Iterate through the table and attempt to claim the first available index.
                for (uint32 index = 0; i < alloc.size(); ++i) {
                    if (alloc.get(i).addr.compare_exchange_strong(cmp, addr)) {
                        return i;
                    }
                }
                
                // Attempt to grow if still possible. Fails possibly if out of memory.
                lock.unlock();
                if (!grow()) return npos;
                lock.lock();
            } while (iterations++ < maxIterations);
            return npos;
        }
        
        void ManagedMemoryTable::grow() {
            if (!growing.test_and_set()) {
                realloc(chunks + 1);
            }
            else {
                
            }
        }
        
        void ManagedMemoryTable::alloc(uint32 chunks) {
            if (buffer) Assert::shouldNotEnter();
            
            this->chunks = chunks;
            buffer = reinterpret_cast<uint8*>(std::malloc(calcBytes(chunks, chunkSize)));
            
            initChunk();
        }
        
        void ManagedMemoryTable::realloc(uint32 chunks) {
            std::unique_lock lock(bufferMutex);
            auto* oldBuffer = buffer;
            
            
            buffer = reinterpret_cast<uint8*>(std::malloc(calcBytes(chunks, chunkSize)));
            
            // Make sure the algorithms recognize unused entries.
            if (chunks > this->chunks) {
                for (uint32 i = oldChunks * chunkSize; i < chunks * chunkSize; ++i) {
                    get(i).addr = nullptr;
                }
            }
            
            this->chunks = chunks;
        }
        
        void ManagedMemoryTable::dealloc() {
            std::unique_lock lock(bufferMutex);
            std::free(buffer);
            buffer = nullptr;
        }
        
        void ManagedMemoryTable::initChunk(uint8* buffer, uint32 chunkIndex, uint32 chunkSize) {
            const uint32 sizeChunkHead  = sizeof(ManagedMemoryTableChunkHead),
                         sizeDescriptor = sizeof(ManagedMemoryDescriptor);
            
            uint8* start = buffer + chunkIndex * (sizeChunkHead + chunkSize * sizeDescriptor);
            new (start) ManagedMemoryTableChunkHead();
            
            start += sizeChunkHead;
            for (uint32 i = 0; i < chunkSize; ++i) {
                new (start + i * sizeDescriptor) ManagedMemoryDescriptor();
            }
        }
        
        void ManagedMemoryTable::copyChunk(const uint8* source, uint8* target, uint32 sourceIndex, uint32 targetIndex, uint32 chunkSize) {
            const uint32 sizeChunkHead  = sizeof(ManagedMemoryTableChunkHead),
                         sizeDescriptor = sizeof(ManagedMemoryDescriptor);
            
            const uint8* sourceStart = source + sourceIndex * (sizeChunkHead + chunkSize * sizeDescriptor) + sizeChunkHead;
            uint8* targetStart = target + targetIndex * (sizeChunkHead + chunkSize * sizeDescriptor);
            
            new (targetStart) ManagedMemoryTableChunkHead();
            targetStart += sizeChunkHead;
            
            for (uint32 i = 0; i < chunkSize; ++i) {
                new (targetStart + i * sizeDescriptor) ManagedMemoryDescriptor(*reinterpret_cast<ManagedMemoryDescriptor*>(sourceStart + i * sizeDescriptor));
            }
        }
        
        uint32 ManagedMemoryTable::calcOffset(uint32 index) const {
            const uint32 sizeDescriptor = sizeof(ManagedMemoryDescriptor),
                         sizeChunkHead  = sizeof(ManagedMemoryTableChunkHead),
                         chunkIndex = index / chunkSize,
                         entryIndex = index % chunkSize;
            return chunkIndex * (chunkSize * sizeDescriptor + sizeChunkHead) + entryIndex * sizeDescriptor;
        }
        
        uint32 ManagedMemoryTable::calcBytes(uint32 chunks, uint32 chunkSize) {
            return chunks * (chunkSize * sizeof(ManagedMemoryDescriptor) + sizeof(ManagedMemoryTableChunkHead));
        }
    }
}
