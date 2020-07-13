////////////////////////////////////////////////////////////////////////////////////
// Implementation of the Neuro Garbage Collector.
// 
//   Object Management Lifecycle
// 
// Whenever a Neuro::Value is reassigned and it previously contained a pointer
// to a managed object, said managed object is flagged for scanning.
// 
// The GC will then in the next Scanning Phase trace the roots until it finds at
// least one living pointer to the object.
// 
// If no such pointer is found, the object is flagged as garbage and will be
// collected in the next Sweep Phase.
// 
// Sweeping may trigger a Compact Phase, which is a rather complicated algorithm.
// At the beginning of the compaction, objects will be predetermined for
// relocation. Next, these objects are moved to their new assigned location and
// the old overhead is updated to redirect indirection resolutions of the
// ManagedMemoryPointer. This rule is in place to avoid stopping the world just
// to update an unknown number old pointers scattered across the entire managed
// memory. Instead, we can now update pointers at our own leisure. New pointers
// to the old data (e.g. through copying) are immediately redirected to point to
// the new address.
// 
// Currently, the garbage collector supports managing native types within limits.
// Native types are not consulted when tracing objects, their pointers are merely
// weak pointers that can be updated after registering them.
// 
// TODO: Track which objects are commonly used together and group them up.
// TODO: Track memory use over the past x minutes in an attempt to predict
//       future memory use.
// -----
// Copyright (c) Kiruse 2018
// License: GPL 3.0
#include <chrono>
#include <utility>
#include <cstring>

#include "GC/NeuroGC.h"
#include "GC/NeuroGC.hpp"
#include "GC/Queue.hpp"

#include "NeuroString.hpp"
#include "NeuroValue.hpp"

namespace Neuro {
    namespace Runtime {
        ////////////////////////////////////////////////////////////////////////
        // Statics
        ////////////////////////////////////////////////////////////////////////
        
        std::mutex gcMainInstanceMutex;
        GCInterface* gcMainInstance = nullptr;
        
        
        ////////////////////////////////////////////////////////////////////////
        // Local Forward Declarations
        ////////////////////////////////////////////////////////////////////////
        
        ManagedMemorySegment* createSegment(uint32 minSize);
        void appendSegment(ManagedMemorySegment* segment, ManagedMemorySegment* chain);
        bool segmentContainsHead(ManagedMemorySegment* segment, ManagedMemoryOverhead* head);
        
        ManagedMemoryOverhead* getFirstOverhead(ManagedMemorySegment* segment);
        ManagedMemoryOverhead* getNextOverhead(ManagedMemoryOverhead* head);
        
        
        ////////////////////////////////////////////////////////////////////////
        // GCInterface
        ////////////////////////////////////////////////////////////////////////
        
        ManagedMemoryPointerBase GCInterface::makePointer(uint32 index, hashT hash) {
            ManagedMemoryPointerBase pointer;
            pointer.tableIndex = index;
            pointer.rowuid = hash;
            return pointer;
        }
        
        void GCInterface::extractPointerData(ManagedMemoryPointerBase pointer, uint32& index, hashT& hash) {
            index = pointer.tableIndex;
            hash  = pointer.rowuid;
        }
        
        
        ////////////////////////////////////////////////////////////////////////
        // RAII
        ////////////////////////////////////////////////////////////////////////
        
        GC::GC()
         : scannersMutex()
         , rootsMutex()
         , markedObjectsMutex()
         , terminate(false)
         , backgroundThread()
         , scanners()
         , dataTable()
         , firstTrivialMemSeg(createSegment(2048))
         , firstNonTrivialMemSeg(createSegment(512))
         , roots()
         , markedObjects()
         , scanInterval(std::chrono::seconds(3))
        {
            // Add our default Object scanner.
            scanners.add(ScannerDelegate::MethodDelegate<GC, &GC::scanForObjects>(this));
            
            // Start up the main thread
            backgroundThread = std::thread(Delegate<void>::MethodDelegate<GC, &GC::threadMain>(this));
        }
        
        GC::~GC() {
            terminate = true;
            backgroundThread.join();
            terminate = false; // Should the GC be restarted afterwards we need to make sure it won't shut down immediately again.
            
            // Trivial memory is easy to clean up. Just free everything in batches!
            ManagedMemorySegment* curr = firstTrivialMemSeg;
            while (curr) {
                ManagedMemorySegment* next = curr->next;
                std::free(curr);
                curr = next;
            }
            firstTrivialMemSeg = nullptr;
            
            // Non-trivial memory is harder to clean up. All valid memory sections
            // must be cleaned up individually!
            curr = firstNonTrivialMemSeg;
            while (curr) {
                ManagedMemorySegment* next = curr->next;
                
                // First allocated object is always immediately behind the
                // segment overhead.
                ManagedMemoryOverhead* head = reinterpret_cast<ManagedMemoryOverhead*>(reinterpret_cast<uint8*>(curr) + sizeof(ManagedMemoryOverhead));
                if (head->destroyDelegate) {
                    head->destroyDelegate.get()(head->getBufferPointer());
                }
                
                std::free(curr);
                curr = next;
            }
            firstNonTrivialMemSeg = nullptr;
        }
        
        
        ////////////////////////////////////////////////////////////////////////
        // Managed Memory Segment Helpers
        ////////////////////////////////////////////////////////////////////////
        
        ManagedMemorySegment* createSegment(uint32 minSize) {
            const uint32 size = std::max<uint32>(sizeof(ManagedMemorySegment) + minSize, 2 * 1024 * 1024); // At least 2MB
            
            // TODO: Optimize the sizes of the chunks of memory based on recent memory usage!
            void* newMemory = std::malloc(size);
            if (!newMemory) return nullptr;
            
            auto* segment = reinterpret_cast<ManagedMemorySegment*>(newMemory);
            segment->next = nullptr;
            segment->allocating.clear();
            segment->compacting = false;
            segment->ptr = reinterpret_cast<uint8*>(segment) + sizeof(ManagedMemorySegment);
            segment->size = size;
            segment->dormant = false;
            
            return segment;
        }
        
        void appendSegment(ManagedMemorySegment* segment, ManagedMemorySegment* chain) {
            ManagedMemorySegment* compare = nullptr;
            while (!chain->next.compare_exchange_strong(compare, segment)) {
                chain = chain->next.load();
				compare = nullptr;
            }
        }
        
        bool segmentContainsHead(ManagedMemorySegment* segment, ManagedMemoryOverhead* head) {
            const intptr_t nSegment = reinterpret_cast<intptr_t>(segment),
                           nHead    = reinterpret_cast<intptr_t>(head);
            return nSegment < nHead && nHead + head->elementSize * head->count + sizeof(ManagedMemoryOverhead) < nSegment + segment->size;
        }
        
        ManagedMemoryOverhead* getFirstOverhead(ManagedMemorySegment* segment) {
            // Due to the pointer type, + 1 adds the entire size of the ManagedMemorySegment struct,
            // as if navigating to the element at index 1.
            // Makes it nice and easy to find the first object.
            return reinterpret_cast<ManagedMemoryOverhead*>(segment + 1);
        }
        
        ManagedMemoryOverhead* getNextOverhead(ManagedMemoryOverhead* head) {
            return reinterpret_cast<ManagedMemoryOverhead*>(reinterpret_cast<uint8*>(head) + head->elementSize * head->count);
        }
        
        
        ////////////////////////////////////////////////////////////////////////
        // Allocation
        ////////////////////////////////////////////////////////////////////////
        
        uint8* allocate_inner(ManagedMemorySegment* chain, uint32 size) {
            // Attempt to find a memory segment that has enough capacity for us still.
            ManagedMemorySegment* segment = chain;
            uint8* addr = nullptr;
            
            while (segment && !addr) {
                // Lock the segment!
                // Uses a spinlock since we don't anticipate at most a few hundred nanoseconds until the lock is released again.
                ManagedMemorySegment::Lock{segment};
                
                // Compaction takes a while, so just move on.
                // If the memory can't hold the requested size, move on too.
                if (segment->compacting || (segment->ptr + size > reinterpret_cast<uint8*>(segment) + segment->size)) {
                    segment = segment->next;
                }
                else {
                    addr = segment->ptr;
                    segment->ptr += size;
                }
            }
            
            // If no suitable segment was found, we need to create a new one.
            if (!segment) {
                // Create segment with at least `size` bytes.
                segment = createSegment(size);
                
                // Failed to allocate memory chunk!
                if (!segment) return nullptr;
                
                addr = segment->ptr;
                segment->ptr += size;
                appendSegment(segment, chain);
            }
            
            return addr;
        }
        
        ManagedMemoryPointerBase GC::allocateTrivial(uint32 elementSize, uint32 count) {
            // Actually allocate the buffer.
            auto* head = reinterpret_cast<ManagedMemoryOverhead*>(allocate_inner(firstTrivialMemSeg, sizeof(ManagedMemoryOverhead) + elementSize * count));
            
            // Create the overhead & initialize with data
            new (head) ManagedMemoryOverhead(elementSize, count);
            head->isTrivial = true;
            
            // Return a managed pointer wrapper.
            return dataTable.addPointer(head);
        }
        
        ManagedMemoryPointerBase GC::allocateNonTrivial(uint32 elementSize, uint32 count, const Delegate<void, void*, const void*>& copyDelegate, const Delegate<void, void*>& destroyDelegate) {
            // Actually allocate the buffer.
            auto* head = reinterpret_cast<ManagedMemoryOverhead*>(allocate_inner(firstNonTrivialMemSeg, sizeof(ManagedMemoryOverhead) + elementSize * count));
            
            // Create the overhead & populate with data
            new (head) ManagedMemoryOverhead(elementSize, count);
            head->isTrivial = false;
            copyDelegate.copyTo(&head->copyDelegate.get());
            destroyDelegate.copyTo(&head->destroyDelegate.get());
            
            // Return a managed pointer wrapper.
            return dataTable.addPointer(head);
        }
        
        Error GC::reallocate(ManagedMemoryPointerBase ptr, uint32 elementSize, uint32 count, bool autocopy) {
            auto* oldHead = ptr.getHeadPointer();
            auto* newHead = reinterpret_cast<ManagedMemoryOverhead*>(allocate_inner(oldHead->isTrivial ? firstTrivialMemSeg : firstNonTrivialMemSeg, sizeof(ManagedMemoryOverhead) + elementSize * count));
            
            new (newHead) ManagedMemoryOverhead(elementSize, count);
            newHead->isTrivial = oldHead->isTrivial;
            if (newHead->isTrivial) {
                if (autocopy) {
                    // IMPORTANT: circumstances predict that new buffer cannot intersect with old buffer, hence
                    // DO NOT use memmove as it comes with a small performance penalty!
                    std::memcpy(newHead->getBufferPointer(), oldHead->getBufferPointer(), oldHead->elementSize * oldHead->count);
                }
            }
            else {
                oldHead->copyDelegate.get().copyTo(&newHead->copyDelegate.get());
                oldHead->destroyDelegate.get().copyTo(&newHead->destroyDelegate.get());
                
                if (autocopy) {
                    newHead->copyDelegate.get()(newHead->getBufferPointer(), oldHead->getBufferPointer());
                }
            }
            
            return dataTable.replacePointer(ptr, newHead);
        }
        
        
        ////////////////////////////////////////////////////////////////////////
        // GCInterface
        ////////////////////////////////////////////////////////////////////////
        
        Error GC::root(Pointer obj) {
            std::scoped_lock{rootsMutex};
            roots.add(obj);
            return NoError::instance();
        }
        
        Error GC::unroot(Pointer obj) {
            std::scoped_lock{rootsMutex};
            roots.remove(obj);
            return NoError::instance();
        }
        
        
        void* GC::resolve(ManagedMemoryPointerBase pointer) {
            return dataTable.get(pointer);
        }
        
        
        ////////////////////////////////////////////////////////////////////////
        // GC Background Thread
        ////////////////////////////////////////////////////////////////////////
        
        void GC::threadMain() {
            uint32 marks = 0;
            
            // Use of .load() method should make it clear to the compiler to
            // not optimize the loop condition away.
            while (!terminate.load()) {
				// std::this_thread::sleep_for(scanInterval);
                // marks += scan();
                // if (marks) {
                //     sweep();
                //     compact();
                //     marks = 0;
                // }
            }
        }
        
        
        ////////////////////////////////////////////////////////////////////////
        // Scan Phase
        ////////////////////////////////////////////////////////////////////////
        
        uint32 GC::scan() {
            StandardHashSet<ManagedMemoryPointerBase> garbage;
            dataTable.collect(garbage);
            scanners(garbage);
            return garbage.count();
        }
        
        void GC::scanForObjects(StandardHashSet<ManagedMemoryPointerBase>& scans) {
            Buffer<Pointer> processList;
            
            // Start with scanning roots first.
            {
                std::scoped_lock{rootsMutex};
                processList = roots;
            }
            
            // Set of objects we've already visited. Avoids cyclic references
            // resulting in infinite loops.
            StandardHashSet<Pointer> visited(processList.begin(), processList.end());
            uint32 numScans = scans.count();
            
            // Iterate through the roots and attempt to find at least one
            // reference to the flagged objects.
            while (processList.length()) {
                Pointer curr = processList.first();
                processList.splice(0);
                
                for (Property& prop : *curr) {
                    if (prop.value.isManagedObject()) {
                        Pointer other = prop.value.getManagedObject();
                        
                        // Remove from trace list, if it is in it!
                        scans.remove(other);
                        
                        // If we've found at least one reference to all trace targets, we're done for good.
                        if (!--numScans) return;
                        
                        // Only process the found object once per phase.
                        if (!visited.contains(other)) {
                            processList.add(other);
                            visited.add(other);
                        }
                    }
                }
            }
            
            // Remove the pointers from the pointer table
            for (auto garbage : scans) {
                dataTable.removePointer(garbage);
            }
            
            {
                std::scoped_lock{markedObjectsMutex};
                markedObjects.add(scans.begin(), scans.end());
            }
        }
        
        
        ////////////////////////////////////////////////////////////////////////
        // Sweep Phase
        ////////////////////////////////////////////////////////////////////////
        
        void GC::sweep() {
            sweep(true);
            sweep(false);
        }
        
        void GC::sweep(bool trivial) {
            Buffer<ManagedMemoryPointerBase> processList, oldMarked;
            {
                std::scoped_lock{markedObjectsMutex};
                oldMarked = std::move(markedObjects);
                processList = oldMarked;
            }
            
            // Clean up the data, distinguishing between trivial and non-trivial data.
            while (processList.length()) {
                auto pointer = processList.first();
                processList.splice(0);
                
                auto* head = GC::getOverhead(pointer);
                
                // Call non-trivial memory's destruction delegate.
                if (!trivial) {
                    head->destroyDelegate.get()(pointer.get());
                }
                
                head->garbageState = EGarbageState::Swept;
            }
        }
        
        
        ////////////////////////////////////////////////////////////////////////
        // Compaction Phase
        ////////////////////////////////////////////////////////////////////////
        
        /**
         * Find the first swept managed memory overhead in the given segment
         * starting at `start`.
         */
        ManagedMemoryOverhead* findSwept(ManagedMemorySegment* segment, ManagedMemoryOverhead* start) {
            auto* head = start;
            while (segmentContainsHead(segment, head) && head->garbageState != EGarbageState::Swept) head = getNextOverhead(head);
            if (!segmentContainsHead(segment, head)) return nullptr;
            return head;
        }
        
        /**
         * Find the first unswept managed memory overhead in the given segment
         * starting at `start`.
         */
        ManagedMemoryOverhead* findUnswept(ManagedMemorySegment* segment, ManagedMemoryOverhead* start) {
            auto* head = start;
            while (segmentContainsHead(segment, head) && head->garbageState == EGarbageState::Swept) head = getNextOverhead(head);
            if (!segmentContainsHead(segment, head)) return nullptr;
            return head;
        }
        
        void GC::compact() {
            compact(true);
            compact(false);
        }
        
        void GC::compact(bool trivial) {
            ManagedMemorySegment* sourceSegment = trivial ? firstTrivialMemSeg : firstNonTrivialMemSeg;
            ManagedMemorySegment* targetSegment = nullptr;
            
            // TODO
        }
        
        
        ////////////////////////////////////////////////////////////////////////////
        // Low Level API
        ////////////////////////////////////////////////////////////////////////////
        
        Error GC::registerMemoryScanner(const Delegate<void, StandardHashSet<ManagedMemoryPointerBase>&>& scanner) {
            scanners += scanner;
            return NoError::instance();
        }
        
        
        ////////////////////////////////////////////////////////////////////////////
        // Main Instance Management
        ////////////////////////////////////////////////////////////////////////////
        
        GCInterface* GC::instance() {
            return gcMainInstance;
        }
        
        MaybeAnError<GC*> GC::init() {
            std::scoped_lock{gcMainInstanceMutex};
            if (gcMainInstance) return InvalidStateError::instance();
            
            auto gc = new GC();
            gcMainInstance = gc;
            return gc;
        }
        
        Error GC::init(GCInterface* instance) {
            std::scoped_lock{gcMainInstanceMutex};
            if (gcMainInstance) return InvalidStateError::instance();
            
            gcMainInstance = instance;
            return NoError::instance();
        }
        
        Error GC::destroy() {
            std::scoped_lock{gcMainInstanceMutex};
            if (!gcMainInstance) return InvalidStateError::instance();
            
            delete gcMainInstance;
            gcMainInstance = nullptr;
            
            return NoError::instance();
        }
    }
}


