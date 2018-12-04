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
#include "GC/ManagedMemoryDescriptor.hpp"
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
            uint32 longterm : 1;
            
            
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
            std::atomic_flag terminate;
            
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
            MulticastDelegate<void, StandardHashSet<ManagedMemoryPointerBase>&> scanners;
            
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
                 * Queue of managed memory sections to scan during the next mark
                 * iteration. The special Queue implementation guarantees that
                 * enqueuing and extraction are wait-free. However, dequeuing is
                 * not implemented, instead we extract and process entire queues
                 * at once.
                 */
                Queue<ManagedMemoryPointerBase> scans;
                
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
                 * Last time short term memory was scanned. Because we scan
                 * about every 3 minutes, lax synchronization rules.
                 * 
                 * Note: General scans are preemptive. Next to these scans
                 * we also use reactive scans timely close to queuing an
                 * arbitrary memory section for scanning.
                 */
                std::chrono::steady_clock::time_point lastGeneralShortTermMemoryScan;
                
                /**
                 * Last time long term memory was scanned. Because we scan
                 * about every 10 minutes, lax synchronization rules.
                 * 
                 * Note: General scans are preemptive. Next to these scans
                 * we also use reactive scans timely close to queuing an
                 * arbitrary memory section for scanning.
                 */
                std::chrono::steady_clock::time_point lastGeneralLongTermMemoryScan;
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
        void gcScanObject(StandardHashSet<ManagedMemoryPointerBase>& scans);
        
        /**
         * Sweep garbage.
         */
        void gcSweep();
        
        /**
         * Patch the gaps from sweeping.
         */
        void gcCompact();
        
        template<bool Trivial>
        void gcCompact(ManagedMemorySegment* chain);
        
        template<bool Trivial>
        void gcCompactInner(ManagedMemorySegment* sourceSegment, ManagedMemorySegment* targetSegment, ManagedMemoryOverhead* head);
        
        /**
         * Update strong and weak pointers to managed memory.
         */
        void updatePointers(ManagedMemoryOverhead* from, ManagedMemoryOverhead* to);
        
        void gcMain();
        
        
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
            segment->longterm = false;
            
            return segment;
        }
        
        void appendSegment(ManagedMemorySegment* segment, ManagedMemorySegment* chain) {
            ManagedMemorySegment* compare = nullptr;
            while (!chain->next.compare_exchange_strong(compare, segment)) {
                chain = chain->next.load();
            }
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
        
        ManagedMemoryPointerBase allocateTrivial(uint32 size) {
            if (!GCGlobals::initialized) return ManagedMemoryPointerBase();
            
            // Account for the overhead as well.
            size += sizeof(ManagedMemoryOverhead);
            
            
            
            return ManagedMemoryPointerBase(head);
        }
        
        ManagedMemoryPointerBase allocateNonTrivial(uint32 size, const Delegate<void, void*, const void*>& copyDelegate, const Delegate<void, void*>& destroyDelegate) {
            if (!GCGlobals::initialized) return ManagedMemoryPointerBase();
            
            // Account for the overhead as well.
            size += sizeof(ManagedMemoryOverhead);
            
            auto* head = reinterpret_cast<ManagedMemoryOverhead*>(allocate_inner(GCGlobals::firstNonTrivialMemSeg, size));
            head->trivial = false;
            head->size = size;
            head->garbage = head->swept = head->relocated = false;
            copyDelegate.copyTo(&head->copy_deleg.get());
            destroyDelegate.copyTo(&head->destroy_deleg.get());
            
            return ManagedMemoryPointerBase(head);
        }
        
        
        ////////////////////////////////////////////////////////////////////
        // Misc. Low Level API
        ////////////////////////////////////////////////////////////////////
        
        void markForScan(ManagedMemoryPointerBase pointer) {
            GCGlobals::LifeCycle::scans.enqueue(pointer);
        }
        
        
        ////////////////////////////////////////////////////////////////////
        // GC Background Thread
        ////////////////////////////////////////////////////////////////////
        
        Error init() {
            using namespace GCGlobals;
            using namespace GCGlobals::LifeCycle;
            
            std::scoped_lock{lifeMutex};
            if (initialized) return InvalidStateError::instance();
            
            compacting = false;
            
            // Start by allocating 2MiB of managed trivial memory
            firstTrivialMemSeg = createSegment(2048);
            
            // as well as 512KiB of managed non-trivial memory.
            // TODO: Support non-trivial managed objects!
            firstNonTrivialMemSeg = createSegment(512);
            
            // Add our default Object scanner.
            scanners.add(ScannerDelegate::FunctionDelegate<gcScanObject>());
            
            // Prevent scanning memory right away.
            lastGeneralShortTermMemoryScan = lastGeneralLongTermMemoryScan = std::chrono::steady_clock::now;
            
            // Start up the main thread
            terminate = false;
            backgroundThread = std::thread(gcMain);
            
            return NoError::instance();
        }
        
        Error destroy() {
            using namespace GCGlobals;
            
            std::scoped_lock{lifeMutex};
            if (!initialized) return InvalidStateError::instance();
            
            terminate.test_and_set();
            backgroundThread.join();
            terminate.clear(); // Should the GC be restarted afterwards we need to make sure it won't shut down immediately again.
            
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
                    gcCompact();
                    marks = 0;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }
            
            // Cleanup happens in GC::destroy()
        }
        
        
        ////////////////////////////////////////////////////////////////////
        // Scan Phase
        ////////////////////////////////////////////////////////////////////
        
        bool shouldScanShortTermMemory() {
            return GCGlobals::LifeCycle::lastGeneralShortTermMemoryScan - std::chrono::steady_clock::now < std::chrono::minutes(3);
        }
        
        bool shouldScanLongTermMemory() {
            return GCGlobals::LifeCycle::lastGeneralLongTermMemoryScan - std::chrono::steady_clock::now < std::chrono::minutes(10);
        }
        
        uint32 gcScan() {
            bool scanShortTerm = shouldScanShortTermMemory(),
                    scanLongTerm  = shouldScanLongTermMemory();
            
            if (scanShortTerm || scanLongTerm) {
                if (scanShortTerm) {
                    // TODO: Scan short term memory!
                    // Discover managed short term memory across all short term segments.
                    // scanners(scans);
                    GCGlobals::LifeCycle::lastGeneralShortTermMemoryScan = std::chrono::steady_clock::now;
                }
                else if (scanLongTerm) {
                    // TODO: Scan long term memory!
                    // Discover managed long term memory across all long term segments.
                    // scanners(scans);
                    GCGlobals::LifeCycle::lastGeneralLongTermMemoryScan = std::chrono::steady_clock::now;
                }
            }
            else if (scans.length(0)) {
                Queue<ManagedMemoryPointerBase> queued;
                GCGlobals::LifeCycle::scans.extract(queued);
                StandardHashSet<ManagedMemoryPointerBase> scans = queued;
                
                scanners(scans);
                return scans.count();
            }
            
            return 0;
        }
        
        void gcScanObject(StandardHashSet<ManagedMemoryPointerBase>& scans) {
            // No need to scan if nothing issued to scan.
            if (!scans.count()) return;
            
            Buffer<Pointer> processList;
            
            // Copy pointers to the roots (thread-safe).
            {
                std::scoped_lock{GCGlobals::rootsMutex};
                processList = GCGlobals::roots;
            }
            
            // Iterate through the roots and attempt to find at least one
            // reference to the flagged objects.
            StandardHashSet<Pointer> visited(processList.begin(), processList.end());
            
            while (processList.length()) {
                Pointer curr = processList.first();
                processList.splice(0);
                
                for (Property& prop : *curr) {
                    if (prop.second.isManagedObject()) {
                        Pointer other = prop.second.getManagedObject();
                        
                        // Remove from trace list, if it is in it!
                        scans.remove(other);
                        
                        // If we've found at least one reference to all trace targets, we're done for good.
                        if (!scans.count()) return;
                        
                        // Only process the found object once per phase.
                        if (!visited.contains(other)) {
                            processList.add(other);
                            visited.add(other);
                        }
                    }
                }
            }
            
            for (auto garbage : scans) {
                garbage->garbage = true;
            }
            
            {
                std::scoped_lock{GCGlobals::LifeCycle::markedMutex};
                GCGlobals::LifeCycle::marked.add(scans.begin(), scans.end());
            }
        }
        
        
        ////////////////////////////////////////////////////////////////////
        // Sweep Phase
        ////////////////////////////////////////////////////////////////////
        
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
                
                // Call non-trivial memory's destruction delegate.
                if (!pointer->trivial) {
                    pointer->destroy_deleg(pointer.get());
                }
                
                pointer.getHeadPointer()->swept = true;
            }
        }
        
        
        ////////////////////////////////////////////////////////////////////
        // Compaction Phase
        ////////////////////////////////////////////////////////////////////
        
        void gcCompact() {
            using namespace GCGlobals;
            
            if (!compacting.test_and_set()) {
                gcCompact<true>();
                gcCompact<false>();
                compacting.clear();
            }
        }
        
        /**
         * Find the first swept managed memory overhead in the given segment
         * starting at `start`.
         */
        ManagedMemoryOverhead* findSwept(ManagedMemorySegment* segment, ManagedMemoryOverhead* start) {
            auto* head = start;
            while (segmentContainsHead(segment, head) && !head->swept) head = getNextOverhead(head);
            if (!segmentContainsHead(segment, head)) return nullptr;
            return head;
        }
        
        /**
         * Find the first unswept managed memory overhead in the given segment
         * starting at `start`.
         */
        ManagedMemoryOverhead* findUnswept(ManagedMemorySegment* segment, ManagedMemoryOverhead* start) {
            auto* head = start;
            while (segmentContainsHead(segment, head) && head->swept) head = getNextOverhead(head);
            if (!segmentContainsHead(segment, head)) return nullptr;
            return head;
        }
        
        template<bool Trivial>
        void gcCompact() {
            ManagedMemorySegment* sourceSegment = Trivial ? GCGlobals::firstTrivialMemSeg : GCGlobals::firstNonTrivialMemSeg;
            ManagedMemorySegment* targetSegment = nullptr;
            
            while (segment) {
                
                
                if (!targetSegment) targetSegment = createSegment(sourceSegment->size - sizeof(ManagedMemorySegment));
                
                // Reuse the now empty segment if possible.
                if (segment->next->size <= segment->size) {
                    fresh = segment;
                }
                
                // Progress to the next segment.
                segment = findSegmentForCompaction(segment);
            }
            
            // TODO: Find segment in need of compaction!
            ManagedMemoryOverhead* head = reinterpret_cast<ManagedMemoryOverhead*>(reinterpret_cast<uint8*>(segment) + sizeof(ManagedMemoryOverhead));
            
            // Number of bytes compacted in the current segment. Because one
            // segment may require multiple iterations in the below loop,
            // needs to be declared here.
            uint32 compactedBytes = 0;
            
            do {
                // Find 
                if (ManagedMemoryOverhead* swept = findSwept(segment, head)) {
                    
                }
                
                // Advance to the next segment.
                else {
                    compactedBytes = 0;
                    segment->ptr -= compactedBytes;
                    segment = segment->next;
                }
            } while(segment);
        }
        
        /**
         * Specialized compaction for trivial types is much simpler: we simply
         * move bytes around.
         */
        template<>
        void gcCompactInner<true>(ManagedMemorySegment* sourceSegment, ManagedMemorySegment* targetSegment, ManagedMemoryOverhead* head) {
            ManagedMemorySegment::Lock{segment};
            
        }
        
        /**
         * Specialized compaction for non-trivial types is a more lengthy
         * operation as we have to consult each 
         */
        template<>
        void gcCompactInner<false>(ManagedMemorySegment* sourceSegment, ManagedMemorySegment* targetSegment, ManagedMemoryOverhead* head) {
            
        }
        
        
        ////////////////////////////////////////////////////////////////////////
        // Neuro::Object::root() & Neuro::Object::unroot()
        ////////////////////////////////////////////////////////////////////////
        
        void Object::root() {
            std::scoped_lock{GC::GCGlobals::rootsMutex};
            GC::GCGlobals::roots.add(Pointer(GC::ManagedMemoryPointerBase::fromObject(this)));
        }
        
        void Object::unroot() {
            std::scoped_lock{GC::GCGlobals::rootsMutex};
            GC::GCGlobals::roots.remove(Pointer(GC::ManagedMemoryPointerBase::fromObject(this)));
        }
        
        Pointer Object::createObject(uint32 knownPropsCount, uint32 propsBufferCount) {
            // TODO: Possibly alter propsBufferCount before we allocate.
            const uint32 totalPropsCount = knownPropsCount + propsBufferCount;
            auto memptr = GC::allocateTrivial(sizeof(Object) + sizeof(Property) * totalPropsCount);
            
            if (memptr) {
                Object* objptr = reinterpret_cast<Object*>(memptr.get());
                new (objptr) Object(totalPropsCount);
            }
            
            return Pointer(memptr);
        }
        
        Pointer Object::recreateObject(Pointer object, uint32 knownPropsCount, uint32 propsBufferCount) {
            const uint32 totalPropsCount = knownPropsCount + propsBufferCount;
            
            // Only recreate if we're actually resizing!
            if (totalPropsCount == object->propCount) return object;
            
            auto memptr = GC::allocateTrivial(sizeof(Object) + sizeof(Property) * totalPropsCount);
            
            if (memptr) {
                Object* objptr = reinterpret_cast<Object*>(memptr.get());
                new (objptr) Object(object, totalPropsCount);
            }
            
            return Pointer(memptr);
        }
    }
}


