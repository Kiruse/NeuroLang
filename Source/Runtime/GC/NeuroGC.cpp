////////////////////////////////////////////////////////////////////////////////
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
#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <utility>

#include "GC/NeuroGC.h"
#include "GC/NeuroGC.hpp"
#include "GC/ManagedMemoryTable.hpp"
#include "GC/Queue.hpp"

#include "Error.hpp"
#include "Maybe.hpp"
#include "NeuroObject.hpp"
#include "NeuroSet.hpp"
#include "NeuroString.hpp"
#include "NeuroValue.hpp"

namespace Neuro {
    namespace Runtime {
        ////////////////////////////////////////////////////////////////////
        // Managed Memory Segment meta data
        // -----
        // Stores some information on a managed memory segment. The memory
        // segments are used in a stack-like fashion, i.e. new memory
        // allocation happens by increasing the `next` pointer by the number
        // of desired bytes. However, unlike the stack, memory can be resized
        // and shifted around accordingly.
        ////////////////////////////////////////////////////////////////////
        
        struct ManagedMemorySegment {
            /**
             * Pointer to the next managed memory segment.
             */
            std::atomic<ManagedMemorySegment*> next;
            
            /**
             * Whether a thread is currently attempting to allocate memory
             * in this segment.
             */
            std::atomic_flag allocating;
            
            /**
             * Whether a thread is currently compacting this memory segment.
             * After compaction, this segment will be destroyed (or reused
             * in further compaction).
             */
            std::atomic_bool compacting;
            
            /**
             * Pointer to the first address guaranteed to be available.
             */
            uint8* ptr;
            
            /**
             * Number of bytes in this memory segment, including the segment
             * overhead.
             */
            uint32 size;
            
            /**
             * Whether this memory segment is reserved for long-term memory
             * that is known not to resize commonly and has survived multiple
             * scans (e.g. roots).
             */
            uint32 dormant : 1; // TODO: Introduce "dormant" memory segments which contain objects that don't change in size anymore.
            
            
            struct Lock {
                ManagedMemorySegment* segment;
                
                Lock(ManagedMemorySegment* segment) : segment(segment) {
                    while (segment->allocating.test_and_set()) std::this_thread::yield();
                }
                Lock(const Lock&) = delete;
                Lock(Lock&&) = delete;
                Lock& operator=(const Lock&) = delete;
                Lock& operator=(Lock&&) = delete;
                ~Lock() {
                    segment->allocating.clear();
                }
            };
        };
        
        
        ////////////////////////////////////////////////////////////////////
        // Globals
        ////////////////////////////////////////////////////////////////////
        
        namespace GCGlobals
        {
            /**
             * Synchronizes access to the initialized flag.
             */
            std::mutex lifeMutex;
            
            /**
             * Whether `init` has been called (and corresponding `destroy` has
             * not yet).
             */
            bool initialized = false;
            
            /**
             * Whether to terminate the GC main thread.
             */
            std::atomic<bool> terminate;
            
            /**
             * The GC's main thread through which memory is managed.
             */
            std::thread backgroundThread;
            
            /**
             * Pointer to this life cycle's first ever managed trivial memory
             * segment.
             * Must be set during initialization and can only be unset during
             * cleanup.
             */
            ManagedMemorySegment* firstTrivialMemSeg = nullptr;
            
            /**
             * Pointer to this life cycle's first ever managed non-trivial
             * memory segment.
             * Must be set during initialization and may only be unset during
             * cleanup. Since we expect less (native) non-trivial objects,
             * should be smaller than `firstTrivialMemSeg`.
             */
            ManagedMemorySegment* firstNonTrivialMemSeg = nullptr;
            
            /**
             * Synchronizes access to the scanners multicast delegate.
             */
            std::mutex scannersMutex;
            
            /**
             * Managed memory scanners to consult during the scan phase.
             * They receive a hash set of pointers to scan for, and are
             * expected to filter out those pointers they have discovered
             * during their scan.
             */
            MulticastDelegate<void, Buffer<ManagedMemoryPointerBase>&> scanners;
            
            /**
             * Table containing descriptors of the various allocated memory
             * sections.
             */
            ManagedMemoryTable dataTable;
            
            /**
             * Synchronizes access to the roots buffer.
             */
            std::mutex rootsMutex;
            
            /**
             * Root objects from which to begin tracing.
             */
            Buffer<Pointer> roots;
            
            namespace LifeCycle
            {
                /**
                 * Synchronizes access to marked.
                 * 
                 * Note: Although we currently do not really need it, we will
                 * likely distribute the workload of the life cycle across
                 * multiple threads for increased performance. But, we'll
                 * need to implement the Runtime's main thread pool first.
                 */
                std::mutex markedMutex;
                
                /**
                 * Managed memory sections that have been marked for sweeping.
                 */
                Buffer<ManagedMemoryPointerBase> marked;
                
                /**
                 * Duration in between scans.
                 */
                std::chrono::milliseconds scanInterval;
            }
        }
        
        
        ////////////////////////////////////////////////////////////////////
        // Local Forward Declarations
        ////////////////////////////////////////////////////////////////////
        
        /**
         * Check the managed memory for unreachable objects.
         */
        uint32 gcScan();
        
        /**
         * Managed Memory Scanner for our Object type.
         */
        void gcScanObject(Buffer<ManagedMemoryPointerBase>& objects);
        
        /**
         * Sweep garbage.
         */
        void gcSweep();
        
        /**
         * Patch the gaps from sweeping.
         */
        void gcCompactAll();
        
        template<bool Trivial>
        void gcCompact(ManagedMemorySegment* chain);
        
        template<bool Trivial>
        void gcCompactInner(ManagedMemorySegment* sourceSegment, ManagedMemorySegment* targetSegment, ManagedMemoryOverhead* head);
        
        /**
         * Update strong and weak pointers to managed memory.
         */
        void updatePointers(ManagedMemoryOverhead* from, ManagedMemoryOverhead* to);
        
        void gcMain();
        
        /**
         * Tests if the given segment contains the specified managed memory overhead.
         */
        bool segmentContainsHead(ManagedMemorySegment* segment, ManagedMemoryOverhead* head);
        
        
        ////////////////////////////////////////////////////////////////////
        // Managed Memory Segment Helpers
        ////////////////////////////////////////////////////////////////////
        
        ManagedMemorySegment* createSegment(uint32 minSize) {
            const uint32 size = std::max<uint32>(sizeof(ManagedMemorySegment) + minSize, 2048);
            
            // TODO: Optimize the sizes of the chunks of memory based on recent memory usage!
            void* newMemory = std::malloc(size);
            if (!newMemory) return nullptr;
            
            auto* segment = reinterpret_cast<ManagedMemorySegment*>(newMemory);
            segment->next = nullptr;
            segment->allocating.clear();
            segment->ptr = reinterpret_cast<uint8*>(segment) + sizeof(ManagedMemorySegment);
            segment->size = size;
            segment->dormant = false;
            
            return segment;
        }
        
        void appendSegment(ManagedMemorySegment* segment, ManagedMemorySegment* chain) {
            ManagedMemorySegment* compare = nullptr;
            while (!chain->next.compare_exchange_strong(compare, segment)) {
                chain = chain->next.load();
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
        
        
        ////////////////////////////////////////////////////////////////////
        // Allocation
        ////////////////////////////////////////////////////////////////////
        
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
        
        ManagedMemoryPointerBase GC::allocateTrivial(uint32 size) {
            if (!GCGlobals::initialized) return ManagedMemoryPointerBase();
            
            // Actually allocate the buffer.
            auto* head = reinterpret_cast<ManagedMemoryOverhead*>(allocate_inner(GCGlobals::firstTrivialMemSeg, size + sizeof(ManagedMemoryOverhead)));
            
            // Create the overhead & initialize with data
            new (head) ManagedMemoryOverhead(size);
            
            // Return a managed pointer wrapper.
            return GCGlobals::dataTable.addPointer(head);
        }
        
        ManagedMemoryPointerBase GC::allocateNonTrivial(uint32 size, const Delegate<void, void*, const void*>& copyDelegate, const Delegate<void, void*>& destroyDelegate) {
            if (!GCGlobals::initialized) return ManagedMemoryPointerBase();
            
            // Actually allocate the buffer.
            auto* head = reinterpret_cast<ManagedMemoryOverhead*>(allocate_inner(GCGlobals::firstNonTrivialMemSeg, size + sizeof(ManagedMemoryOverhead)));
            
            // Create the overhead & populate with data
            new (head) ManagedMemoryOverhead(size);
            copyDelegate.copyTo(&head->copyDelegate.get());
            destroyDelegate.copyTo(&head->destroyDelegate.get());
            
            // Return a managed pointer wrapper.
            return GCGlobals::dataTable.addPointer(head);
        }
        
        
        ////////////////////////////////////////////////////////////////////
        // GC Background Thread
        ////////////////////////////////////////////////////////////////////
        
        Error GC::init() {
            using namespace GCGlobals;
            using namespace GCGlobals::LifeCycle;
            
            std::scoped_lock{lifeMutex};
            if (initialized) return InvalidStateError::instance();
            
            // Start by allocating 2MiB of managed trivial memory
            firstTrivialMemSeg = createSegment(2048);
            
            // as well as 512KiB of managed non-trivial memory.
            // TODO: Support non-trivial managed objects!
            firstNonTrivialMemSeg = createSegment(512);
            
            // Add our default Object scanner.
            scanners.add(ScannerDelegate::FunctionDelegate<gcScanObject>());
            
            // Setup next scan time.
            scanInterval = std::chrono::seconds(3);
            
            // Start up the main thread
            terminate = false;
            backgroundThread = std::thread(gcMain);
            
            return NoError::instance();
        }
        
        Error GC::destroy() {
            using namespace GCGlobals;
            
            std::scoped_lock{lifeMutex};
            if (!initialized) return InvalidStateError::instance();
            
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
                
                // TODO: Support non-trivial managed objects!
                
                std::free(curr);
                curr = next;
            }
            firstNonTrivialMemSeg = nullptr;
            
            initialized = false;
            
            return NoError::instance();
        }
        
        void gcMain() {
            // Init happens in GC::init()
            
            uint32 marks = 0;
            
            // Use of .load() method should make it clear to the compiler to
            // not optimize the loop condition away.
            while (!GCGlobals::terminate.load()) {
                // TODO: Improve upon this barbaric stop-the-world GC algorithm.
                marks += gcScan();
                if (marks) {
                    gcSweep();
                    gcCompactAll();
                    marks = 0;
                }
                std::this_thread::sleep_for(GCGlobals::LifeCycle::scanInterval);
            }
            
            // Cleanup happens in GC::destroy()
        }
        
        
        ////////////////////////////////////////////////////////////////////
        // Scan Phase
        ////////////////////////////////////////////////////////////////////
        
        uint32 gcScan() {
            Buffer<ManagedMemoryPointerBase> pointers = GCGlobals::dataTable.getAllPointers();
            GCGlobals::scanners(pointers);
            return 0; // TODO: Proper return code?
        }
        
        void gcScanObject(Buffer<ManagedMemoryPointerBase>& scans) {
            // No need to scan if nothing issued to scan.
            if (!scans.length()) return;
            
            // List of objects we need to scan.
            Buffer<Pointer> processList;
            
            // Start with scanning roots first.
            {
                std::scoped_lock{GCGlobals::rootsMutex};
                processList = GCGlobals::roots;
            }
            
            // Set of objects we've already visited. Avoids cyclic references
            // resulting in infinite loops.
            StandardHashSet<Pointer> visited(processList.begin(), processList.end());
            
            // Iterate through the roots and attempt to find at least one
            // reference to the flagged objects.
            while (processList.length()) {
                Pointer curr = processList.first();
                processList.splice(0);
                
                for (Property& prop : *curr) {
                    if (prop.second.isManagedObject()) {
                        Pointer other = prop.second.getManagedObject();
                        
                        // Remove from trace list, if it is in it!
                        scans.remove(other);
                        
                        // If we've found at least one reference to all trace targets, we're done for good.
                        if (!scans.length()) return;
                        
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
                GCGlobals::dataTable.removePointer(garbage);
            }
            
            {
                std::scoped_lock{GCGlobals::LifeCycle::markedMutex};
                GCGlobals::LifeCycle::marked.add(scans.begin(), scans.end());
            }
        }
        
        
        ////////////////////////////////////////////////////////////////////
        // Sweep Phase
        ////////////////////////////////////////////////////////////////////
        
        template<bool Trivial>
        void gcSweep() {
            using namespace GCGlobals::LifeCycle;
            
            Buffer<ManagedMemoryPointerBase> processList, oldMarked;
            {
                std::scoped_lock{markedMutex};
                oldMarked = std::move(marked);
                processList = oldMarked;
            }
            
            // Clean up the data, distinguishing between trivial and non-trivial data.
            while (processList.length()) {
                auto pointer = processList.first();
                processList.splice(0);
                
                auto* head = GC::getOverhead(pointer);
                
                // Call non-trivial memory's destruction delegate.
                if (!Trivial) {
                    head->destroyDelegate.get()(pointer.get());
                }
                
                head->garbageState = EGarbageState::Swept;
            }
        }
        
        void gcSweep() {
            gcSweep<true>();
            gcSweep<false>();
        }
        
        
        ////////////////////////////////////////////////////////////////////
        // Compaction Phase
        ////////////////////////////////////////////////////////////////////
        
        template<bool> void gcCompact();
        
        void gcCompactAll() {
            using namespace GCGlobals;
            
            gcCompact<true>();
            gcCompact<false>();
        }
        
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
        
        template<bool Trivial>
        void gcCompact() {
            ManagedMemorySegment* sourceSegment = Trivial ? GCGlobals::firstTrivialMemSeg : GCGlobals::firstNonTrivialMemSeg;
            ManagedMemorySegment* targetSegment = nullptr;
            
            
        }
        
        /**
         * Specialized compaction for trivial types is much simpler: we simply
         * move bytes around.
         */
        template<>
        void gcCompactInner<true>(ManagedMemorySegment* sourceSegment, ManagedMemorySegment* targetSegment, ManagedMemoryOverhead* head) {
            // TODO
        }
        
        /**
         * Specialized compaction for non-trivial types is a more lengthy
         * operation as we have to consult each 
         */
        template<>
        void gcCompactInner<false>(ManagedMemorySegment* sourceSegment, ManagedMemorySegment* targetSegment, ManagedMemoryOverhead* head) {
            // TODO
        }
        
        
        ////////////////////////////////////////////////////////////////////////
        // Low Level API
        ////////////////////////////////////////////////////////////////////////
        
        Error GC::registerMemoryScanner(const Delegate<void, Buffer<ManagedMemoryPointerBase>&>& scanner) {
            GCGlobals::scanners += scanner;
            return NoError::instance();
        }
        
        
        ////////////////////////////////////////////////////////////////////////
        // Neuro::Object methods with direct 
        ////////////////////////////////////////////////////////////////////////
        
        void Object::root() {
            std::scoped_lock{GCGlobals::rootsMutex};
            GCGlobals::roots.add(self);
        }
        
        void Object::unroot() {
            std::scoped_lock{GCGlobals::rootsMutex};
            GCGlobals::roots.remove(self);
        }
        
        Pointer Object::createObject(uint32 knownPropsCount, uint32 propsBufferCount) {
            // TODO: Possibly alter propsBufferCount before we allocate.
            const uint32 totalPropsCount = knownPropsCount + propsBufferCount;
            
            auto rawptr = GC::allocateTrivial(sizeof(Object) + sizeof(Property) * totalPropsCount);
            if (!rawptr) return Pointer();
            
            Pointer self(rawptr);
            new (self.get()) Object(self, totalPropsCount);
            return self;
        }
        
        Pointer Object::recreateObject(Pointer object, uint32 knownPropsCount, uint32 propsBufferCount) {
            const uint32 totalPropsCount = knownPropsCount + propsBufferCount;
            
            // Only recreate if we're actually resizing!
            if (totalPropsCount == object->propCount) return object;
            
            auto rawptr = GC::allocateTrivial(sizeof(Object) + sizeof(Property) * totalPropsCount);
            if (!rawptr) return Pointer();
            
            Pointer self(rawptr);
            new (self.get()) Object(self, object, totalPropsCount);
            return self;
        }
    }
}


