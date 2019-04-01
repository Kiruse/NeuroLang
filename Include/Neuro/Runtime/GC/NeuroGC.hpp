////////////////////////////////////////////////////////////////////////////////
// The lowest possible interface of our Garbage Collector. Usually you'll rather
// avoid working with these functions as calling them directly and improperly
// has high chances of breaking your code.
// 
// Various other indirect interfaces tie into the Garbage Collector and allow for
// a significantly more convenient and safer use.
// 
// The Low Level Interface allows us to implement and embed custom types into the
// Garbage Collector, but this requires at least a custom scanning algorithm.
// The GC traces arbitrary managed memory through pointers on known data structures,
// not through any special pointer format. As an example, the objects have a very
// specific memory layout which allows the GC to determine where pointers are
// and what they point to. Any other approach would require us to describe a
// certain data structure, or consult the object in question directly asking it
// to check if it contains a given pointer, which is slow and outside our control.
// This approach allows us to drastically optimize the performance of the scanners.
// -----
// Copyright (c) Kiruse 2018
// License: GPL 3.0
#pragma once

#pragma warning(push)

// [...] needs to have dll-interface to be used by clients of [...]
#pragma warning(disable: 4251)

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <thread>
#include <utility>

#include "DLLDecl.h"
#include "Delegate.hpp"
#include "Error.hpp"
#include "ManagedMemoryTable.hpp"
#include "MaybeAnError.hpp"
#include "Misc.hpp"
#include "NeuroObject.hpp"
#include "NeuroSet.hpp"

namespace Neuro {
    namespace Runtime {
        ////////////////////////////////////////////////////////////////////
        // Helper Functions
        ////////////////////////////////////////////////////////////////////
        
        template<typename T>
        void copyNonTrivialType(void* target, const void* source) {
            new (reinterpret_cast<T*>(target)) T(*reinterpret_cast<T*>(source));
        }
        
        template<typename T>
        void destroyNonTrivialType(void* target) {
            reinterpret_cast<T*>(target)->~T();
        }
        
        
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
            
            
            /**
             * Yielding spinlock for allocation within this represented segment.
             */
            struct NEURO_API Lock {
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
        
        
        /**
         * Interface that other parts of the Runtime use, e.g. Object.
         */
        class NEURO_API GCInterface
        {
        public:
            /**
             * Allocate `size` bytes of managed trivial memory.
             * 
             * Trivial memory differs from non-trivial memory in that move and
             * destruction operations can be performed in bulk.
             */
            virtual ManagedMemoryPointerBase allocateTrivial(uint32 elementSize, uint32 count) = 0;
            
            /**
             * Allocate `size` bytes of managed non-trivial memory.
             * 
             * When the GC moves or destroys the allocated non-trivial memory,
             * it consults the provided delegates.
             * 
             * Non-trivial memory differs from trivial memory in that its members
             * require per-member copy and destruction operations.
             */
            virtual ManagedMemoryPointerBase allocateNonTrivial(uint32 elementSize, uint32 count, const Delegate<void, void*, const void*>& copyDeleg, const Delegate<void, void*>& destroyDeleg) = 0;
            
            /**
             * Reallocate the buffer underlying the given pointer such that all
             * instances of the buffer need not be changed, but the actual buffer
             * is migrated to another memory region.
             * 
             * Since the original buffer should contain all the necessary
             * information, this method should work with both trivial and
             * non-trivial buffers.
             */
			virtual Error reallocate(ManagedMemoryPointerBase ptr, uint32 size, uint32 count, bool autocopy = true) = 0;
            
            /**
             * Roots the given managed object. The implementation may assume
             * that the object is actually managed by this GC.
             */
            virtual Error root(Pointer obj) = 0;
            
            /**
             * Unroots the given managed object.
             */
            virtual Error unroot(Pointer obj) = 0;
            
            /**
             * Resolves the specified pointer returning a native pointer to the
             * start of the associated buffer.
             */
            virtual void* resolve(ManagedMemoryPointerBase pointer) = 0;
            
        protected:
            /**
             * Helper function for non-ManagedMemoryTable based GC systems to
             * create a ManagedMemoryPointerBase from a given memory address.
             * 
             * Hint: Pointers use 8 or 12 bytes of data. By design, one 32-bit
             * integer describes the index of the pointer within a table, and
             * another 32 or 64 bits store the pointer's unique hash, allowing
             * us to identify obsolete and out of date pointers.
             * 
             * However, one is not bound to these design choices as the
             * GCInterface exposes the `resolve` method as well. One might simply
             * reinterpret the bytes and store a pointer directly instead.
             */
            ManagedMemoryPointerBase makePointer(uint32 index, hashT hash);
            
            /**
             * Inverse of `makePointer`. While `resolve` allows us to specify
             * which ManagedMemoryTable to use, this helper allows us to not
             * use any such table to begin with.
             */
            void extractPointerData(ManagedMemoryPointerBase pointer, uint32& index, hashT& hash);
        };
        
        
        /**
         * The GC resembles a static class of C#. It does not permit instantiation
         * in any way and all of its methods are static. This helps us granting
         * all its components friend access to data structures without having to
         * keep long, explicit lists.
         */
        class NEURO_API GC : public GCInterface
        {
        public:    // Types
            using ScannerDelegate = Delegate<void, StandardHashSet<ManagedMemoryPointerBase>&>;
            
        protected: // Fields
            // TODO: Implement wrappers for std types to ensure library interface consistency.
            // This currently does not enjoy high priority as they are protected
            // and only directly accessed by deriving classes.
            std::mutex scannersMutex;
            std::mutex rootsMutex;
            std::mutex markedObjectsMutex;
            
            std::atomic_bool terminate;
            std::thread backgroundThread;
            
            MulticastDelegate<void, StandardHashSet<ManagedMemoryPointerBase>&> scanners;
            
            ManagedMemoryTable dataTable;
            ManagedMemorySegment* firstTrivialMemSeg;
            ManagedMemorySegment* firstNonTrivialMemSeg;
            
            Buffer<Pointer> roots;
            Buffer<ManagedMemoryPointerBase> markedObjects;
            std::chrono::milliseconds scanInterval;
            
        public:    // RAII
            GC();
            GC(const GC&) = delete;
            GC(GC&&) = delete;
            GC& operator=(const GC&) = delete;
            GC& operator=(GC&&) = delete;
            virtual ~GC();
            
        public:    // GCInterface
            virtual ManagedMemoryPointerBase allocateTrivial(uint32 size, uint32 count) override;
            virtual ManagedMemoryPointerBase allocateNonTrivial(uint32 size, uint32 count, const Delegate<void, void*, const void*>& copyDeleg, const Delegate<void, void*>& destroyDeleg) override;
            
            virtual Error reallocate(ManagedMemoryPointerBase ptr, uint32 size, uint32 count, bool autocopy = true) override;
            
            virtual Error root(Pointer obj) override;
            virtual Error unroot(Pointer obj) override;
            
            virtual void* resolve(ManagedMemoryPointerBase pointer) override;
            
            
        public:    // Methods
            /**
             * Allocate managed memory for a specific data type, optionally
             * specifying whether it should be treated as trivial data even if
             * it's not. The latter might be useful if the data type may not be
             * trivial, but trivially copyable, e.g. creates only flat copies.
             * Neuro's Objects are one such example.
             */
            template<typename T>
            ManagedMemoryPointer<T> allocate(bool trivial = std::is_trivial_v<T>) {
                if (trivial) {
                    return ManagedMemoryPointer<T>(allocateTrivial(sizeof(T)));
                }
                return ManagedMemoryPointer<T>(allocateNonTrivial(sizeof(T), Delegate<void, void*, const void*>::FunctionDelegate<copyNonTrivialType<T>>(), Delegate<void, void*>::FunctionDelegate<destroyNonTrivialType<T>>()));
            }
            
            /**
             * Registers a new scanner to call during the scan phase. It is
             * expected that the scanner removes found pointers from the delegate's
             * passed in argument.
             * 
             * The scanner for our Object type is one such scanner.
             */
            Error registerMemoryScanner(const Delegate<void, StandardHashSet<ManagedMemoryPointerBase>&>& scanner);
            
            
        protected: // Life Cycle
            virtual void threadMain();
            virtual uint32 scan();
            virtual void scanForObjects(StandardHashSet<ManagedMemoryPointerBase>& pointers);
            virtual void sweep();
            virtual void sweep(bool trivial);
            virtual void compact();
            virtual void compact(bool trivial);
            
            
        public:    // Statics
            /**
             * Gets the current main instance. May or may not exist. Initialize
             * with one of either `init` static methods first, and clear with
             * `destroy`.
             */
            static GCInterface* instance();
            
            /**
             * Creates a new garbage collector instance using the default
             * implementation, sets it as main instance, and returns it, unless
             * an error occurred.
             */
            static MaybeAnError<GC*> init();
            
            /**
             * Sets the given garbage collector instance as main instance.
             */
            static Error init(GCInterface* instance);
            
            /**
             * Destroys & frees the current main instance if any.
             */
            static Error destroy();
            
            /**
             * Helper method to retrieve the overhead pointer from any managed
             * memory pointer.
             */
            static ManagedMemoryOverhead* getOverhead(ManagedMemoryPointerBase ptr) {
                return ptr.getHeadPointer();
            }
        };
    }
}

#pragma warning(pop)
