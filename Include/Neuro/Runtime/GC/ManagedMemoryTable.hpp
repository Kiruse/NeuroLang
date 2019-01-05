////////////////////////////////////////////////////////////////////////////////
// Table storing pointers to the various managed objects. The table itself is
// divided into pages of 100 entries, and 10 pages are grouped together and
// stored in a contiguous memory buffer. Every page tracks how many items are
// used. This allows us to more quickly search for an appropriate slot in the
// entire table.
// 
// Using the table concurrent in-place compaction becomes a viable strategy as
// it allows us to copy the object to a new location, then update a single
// pointer in order to update all references altogether.
// 
// Prior, the primary issue was that updating every single reference across all
// managed and unmanaged objects would require a rather extremely optimized
// algorithm, and a mark-and-copy policy rather than mark-and-compact. This
// originates in our ideom principle of full concurrency support: the runtime
// fully supports concurrent threads of execution without any stop-the-world
// interruptions for garbage collection.
// 
// TODO: Optimize for systems where at least one of the atomics is not lock-less.
// Most importantly the sections' ptrs array needs to be mutex-less.
// -----
// Copyright (c) Kiruse 2018
// License: GPL 3.0

#pragma once

#include "DLLDecl.h"
#include "Error.hpp"
#include "ManagedMemoryPointer.hpp"
#include "ManagedMemoryOverhead.hpp"
#include "Numeric.hpp"

#define NEURO_MANAGEDMEMORYTABLESECTION_SIZE 100
#define NEURO_MANAGEDMEMORYTABLEPAGE_SIZE 10

namespace Neuro {
    namespace Runtime
    {
        struct ManagedMemoryTableRow
        {
            /**
             * Actual underlying physical address. Assumed to point to managed
             * memory.
             */
            std::atomic<ManagedMemoryOverhead*> ptr;
            
            /**
             * Locally unique ID of the row, designed to minimize clashing
             * potential. This is specifically used to account for weak pointers
             * outside of the managed memory realm to ensure that the pointer
             * they expect is still the same, even if the row has been reclaimed
             * and reused.
             */
            hashT uid;
            
            ManagedMemoryTableRow() : ptr(nullptr), uid(0) {}
        };
        
        struct ManagedMemoryTableSection
        {
            /**
             * The pointer is indicative for open rows in the table. If not null,
             * the row is already in use.
             */
            ManagedMemoryTableRow rows[100];
            
            /**
             * Number of occupied section entries. Used as a heuristic when
             * searching for an unused row.
             */
            std::atomic<uint8> numOccupied;
            
            ManagedMemoryTableSection() : numOccupied(0) {}
        };
        
        struct ManagedMemoryTablePage
        {
            /**
             * Sections in this page.
             */
            ManagedMemoryTableSection sections[10];
            
            /**
             * Pointer to the next page in case this page has become full.
             */
            std::atomic<ManagedMemoryTablePage*> nextPage;
            
            ManagedMemoryTablePage() : nextPage(nullptr) {}
        };
        
        /**
         * The entire table that stores and manages our pointers to managed
         * memory. The ManagedMemoryPointerBase refers to such a table's row
         * by index.
         */
        class ManagedMemoryTable
        {
            /**
             * First page in this table.
             */
            ManagedMemoryTablePage firstPage;
            
            /**
             * Next salt value for calculation of a hash-based UID.
             */
            std::atomic<uint32> uidsalt;
            
        public:
            ManagedMemoryTable();
            
            /**
             * Assumes the given pointer points to valid managed memory, adds it
             * to the table, and returns a managed memory pointer.
             */
            ManagedMemoryPointerBase addPointer(ManagedMemoryOverhead* addr);
            
            /**
             * Replaces the underlying address of the given managed memory pointer
             * with a new address.
             * 
             *   Implementer's note:
             * When moving data from one location to another whilst both overlap,
             * in order to not obstruct concurrency, it is strongly recommended
             * to move to a temporary buffer first before moving to the overlapping
             * locations, replacing the underlying address twice.
             */
            Error replacePointer(const ManagedMemoryPointerBase& ptr, ManagedMemoryOverhead* newAddr);
            
            /**
             * Removes the corresponding address from the table, freeing up the
             * table row.
             */
            Error removePointer(const ManagedMemoryPointerBase& ptr);
            
            /**
             * Gets the pointer corresponding to the actual data behind the
             * managed memory pointer.
             */
            void* get(const ManagedMemoryPointerBase& ptr) const;
            
            template<typename T>
            T* get(const ManagedMemoryPointer<T>& ptr) const
            {
                return reinterpret_cast<T*>((const ManagedMemoryPointerBase)ptr);
            }
            
            /**
             * Gets pointers to all elements listed in the table.
             */
            Buffer<ManagedMemoryPointerBase> getAllPointers() const;
            
        protected:
            /**
             * Find a table section from the entire table that has capacity for
             * our address. Due to lockless asynchronicity, this may or may not
             * be the first possible section.
             * 
             * If no section could be found during our iteration, a new section
             * will be created. Afterwards the algorithm will attempt to update
             * the last page to point to this new page. If another thread just
             * so happened to do this before us, the newly created page will be
             * freed again and the preexisting new thread will be searched
             * instead. If afterwards we still could not find an appropriate
             * section, this paragraph is repeated.
             * 
             * In general, true parallelism should allow up to n threads - where
             * n is the number of logical cores - to run reliably without
             * locking each other out to the point that a thread cannot allocate
             * memory. Even beyond that number it's highly unlikely we'll lock
             * dead.
             */
            void findSectionForInsert(ManagedMemoryOverhead* addr, ManagedMemoryTablePage*& page, ManagedMemoryTableSection*& section, uint32& pageIndex, uint32& sectionIndex);
            
            /**
             * Finds a table section from the specified page that has capacity
             * for our address. Due to lockless asynchronicity, this may or may
             * not be the first possible section.
             * 
             * If no such section could be found, returns a nullptr.
             */
            uint32 findSectionForInsert(ManagedMemoryTablePage* page, ManagedMemoryOverhead* addr);
            
            /**
             * Gets the row stored behind the given index.
             */
            bool get_internal(uint32 index, ManagedMemoryTablePage*& page, ManagedMemoryTableSection*& section, ManagedMemoryTableRow*& row);
        };
    }
}
