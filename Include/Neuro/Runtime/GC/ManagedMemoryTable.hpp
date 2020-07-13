////////////////////////////////////////////////////////////////////////////////
// Table storing pointers to the various managed objects.
// 
// Table structure:
// + Table
// |-+ Ledger
//   |-+ 5 Pages
//     |-+ 10 Sections
//       |-- 100 Rows (pointers + minor overhead)
// 
// One ledger is stored directly in the stack of the table itself. One additional
// ledger is stored on heap. Further ledgers are created at runtime as needed.
// 
// All nested structures are stack-based, with the exception of ledgers.
// 
// Ledgers *must* be heap-based in order to permit active users (threads) to use
// them while the GC thread is adding additional ledgers. This fixes the number
// of indirections to 3: 1 = list of ledgers, 2 = list of pages, 3 = stored pointer.
// Additionally, LL-Cache may keep the list of ledgers, which is not possible
// with a linked list.
// 
// It remains to be seen whether this is efficient enough. An alternative
// solution would be implementation of a many-readers-one-writer lock system
// which would mean a stop-the-world (for a few millis) while replacing the
// ledgers, but at the same time copying all ledgers performs at O(n).
// 
// TODO: Optimize for systems where at least one of the atomics is not lock-less.
// Most importantly the sections' ptrs array needs to be mutex-less.
// -----
// Copyright (c) Kiruse 2018
// License: GPL 3.0

#pragma once

#pragma warning(push)
#pragma warning(disable: 4251)

#include <atomic>
#include <utility>
#include <cstring>

#include "DLLDecl.h"
#include "Error.hpp"
#include "ManagedMemoryPointer.hpp"
#include "ManagedMemoryOverhead.hpp"
#include "NeuroSet.hpp"
#include "Numeric.hpp"
#include "Concurrency/ReverseSemaphore.hpp"

#define NEURO_MANAGEDMEMORYTABLE_RECORDS_PER_PAGE 1000

namespace Neuro {
    namespace Runtime
    {
        struct ManagedMemoryTableRecord
        {
            /**
             * Actual underlying physical address. Assumed to point to managed
             * memory.
             */
            ManagedMemoryOverhead* ptr;
            
            /**
             * Locally unique ID of the row, designed to minimize clashing
             * potential. This is specifically used to account for weak pointers
             * outside of the managed memory realm to ensure that the pointer
             * they expect is still the same, even if the row has been reclaimed
             * and reused.
             */
            hashT uid;
        };
        
        struct ManagedMemoryTablePage
        {
            ManagedMemoryTableRecord records[NEURO_MANAGEDMEMORYTABLE_RECORDS_PER_PAGE];
        };
        
        struct ManagedMemoryTableRange
        {
            std::atomic<uint32> start;
            uint32 end;
            
            ManagedMemoryTableRange() = default;
            ManagedMemoryTableRange(uint32 start, uint32 end) : start(start), end(end) {}
            
            uint32 claim() {
                uint32 index = start.fetch_add(1);
                if (index >= end) {
                    start.fetch_sub(1);
                    return npos;
                }
                return index;
            }
        };
        
        
        /**
         * Specialized RAII allocator designed for ManagedMemoryTablePages. Main
         * distinction is that data is treated as bytewise copyable raw data.
         */
        struct ManagedMemoryTablePageAllocator {
        public: // Properties
            ManagedMemoryTablePage* m_buffer;
            uint32 m_size;
            
        public: // RAII
            ManagedMemoryTablePageAllocator() : m_buffer(nullptr), m_size(0) {}
            ManagedMemoryTablePageAllocator(uint32 desiredSize) : m_buffer(alloc(desiredSize)), m_size(desiredSize) {
                std::memset(m_buffer, 0, sizeof(ManagedMemoryTablePage) * desiredSize);
            }
            ManagedMemoryTablePageAllocator(const ManagedMemoryTablePageAllocator& other) : m_buffer(alloc(other.m_size)), m_size(other.m_size) {}
            ManagedMemoryTablePageAllocator(ManagedMemoryTablePageAllocator&& other) : m_buffer(other.m_buffer), m_size(other.m_size) {
                other.m_buffer = nullptr;
                other.m_size = 0;
            }
            ManagedMemoryTablePageAllocator& operator=(const ManagedMemoryTablePageAllocator& other) {
                if (m_buffer) delete[] m_buffer;
                m_buffer = alloc(other.m_size);
                m_size   = other.m_size;
                copy(0, other.m_buffer, m_size);
                return *this;
            }
            ManagedMemoryTablePageAllocator& operator=(ManagedMemoryTablePageAllocator&& other) {
                if (m_buffer) delete[] m_buffer;
                m_buffer = other.m_buffer;
                m_size   = other.m_size;
                other.m_buffer = nullptr;
                other.m_size   = 0;
                return *this;
            }
            ~ManagedMemoryTablePageAllocator() {
                if (m_buffer) delete[] m_buffer;
                m_buffer = nullptr;
                m_size = 0;
            }
            
        public: // Interface
            void resize(uint32 desiredSize) {
                if (m_buffer && desiredSize != m_size) {
                    ManagedMemoryTablePage* tmp = m_buffer;
                    m_buffer = alloc(desiredSize);
                    if (m_buffer) {
                        uint32 copysize = std::min(desiredSize, m_size);
                        std::memset(m_buffer + copysize, 0, std::max(desiredSize, m_size) - copysize);
                        std::memcpy(m_buffer, tmp, copysize);
                        m_size = desiredSize;
                        delete[] tmp;
                    }
                    else {
                        m_size = 0;
                    }
                }
            }
            
            void destroy(uint32 index, uint32 count) {}
            
            void copy(uint32 index, const ManagedMemoryTablePage* source, uint32 count) {
                if (m_buffer) {
                    count = std::min(count, m_size - index) * sizeof(ManagedMemoryTablePage);
                    std::memcpy(m_buffer, source, count);
                }
            }
            void copy(uint32 to, uint32 from, uint32 count) {
                if (m_buffer) {
                    count = std::min(m_size - to, count);
                    count = std::min(m_size - from, count);
                    std::memmove(m_buffer + to, m_buffer + from, count * sizeof(ManagedMemoryTablePage));
                }
            }
            
            ManagedMemoryTablePage* get(uint32 index) { return m_buffer + index; }
            const ManagedMemoryTablePage* get(uint32 index) const { return m_buffer + index; }
            
            uint32 size() const { return m_size; }
            uint32 actual_size() const { return m_size; }
            uint32 numBytes() const { return m_size * sizeof(ManagedMemoryTablePage); }
            
            ManagedMemoryTablePage* data() { return m_buffer; }
            const ManagedMemoryTablePage* data() const { return m_buffer; }
            
        protected: // Static helpers
            static ManagedMemoryTablePage* alloc(uint32 desiredSize) {
                if (!desiredSize) return nullptr;
                return new ManagedMemoryTablePage[desiredSize];
            }
        };
        
        
        /**
         * The entire table that stores and manages our pointers to managed
         * memory. The ManagedMemoryPointerBase refers to such a table's row
         * by index.
         */
        class NEURO_API ManagedMemoryTable
        {
        public:    // Types
            class Iterator {
            private: // Properties
                const ManagedMemoryTable* table;
                uint32 tableIndex;
                
            public:  // RAII
                Iterator(const ManagedMemoryTable* table, uint32 tableIndex) : table(table), tableIndex(tableIndex) {}
                Iterator(const Iterator& other) : table(other.table), tableIndex(other.tableIndex) {}
                Iterator& operator=(const Iterator& other) {
                    table = other.table;
                    tableIndex = other.tableIndex;
                    return *this;
                }
                ~Iterator() = default;
                
            public:  // Methods
                Iterator& operator++();
                
                Iterator operator++(int) {
                    Iterator copy(*this);
                    ++*this;
                    return copy;
                }
                
                Iterator& operator--();
                
                Iterator operator--(int) {
                    Iterator copy(*this);
                    --*this;
                    return copy;
                }
                
                ManagedMemoryPointerBase operator*() const {
                    ManagedMemoryPointerBase ptr;
                    ManagedMemoryTableRecord* record = table->getRecord(tableIndex);
                    ptr.tableIndex = tableIndex;
                    ptr.rowuid = record->uid;
                    return ptr;
                }
                
                ManagedMemoryPointerBase get() const {
                    return **this;
                }
                
                bool operator==(const Iterator& other) const {
                    return table == other.table && tableIndex == other.tableIndex;
                }
                
                bool operator!=(const Iterator& other) const {
                    return !(*this==other);
                }
            };
        
        private:   // Properties
            /**
             * Whether the table should be expanded.
             */
            std::atomic<bool> shouldExpand;
            
            /**
             * Whether we should scan for gaps eventually.
             */
            std::atomic<bool> shouldScanGaps;
            
            /**
             * Underlying many-readers-one-writer sync mechanism.
             */
            mutable Concurrency::ReverseSemaphore semaphore;
            
            Buffer<ManagedMemoryTablePage, ManagedMemoryTablePageAllocator> pages;
            
            /**
             * Stores the index of the next record to be inserted.
             */
            std::atomic<uint32> nextRecordIdx;
            
            /**
             * Many-readers-one-writer sync mechanism for gaps detection.
             */
            mutable Concurrency::ReverseSemaphore gapsSemaphore;
            
            /**
             * Pointer to the buffer of currently available ranges. May also
             * contain exhausted ranges.
             */
            ManagedMemoryTableRange* gaps;
            
            /**
             * Number of currently available ranges.
             */
            uint32 numGaps;
            
            /**
             * Next salt value for calculation of a hash-based UID.
             */
            std::atomic<uint32> uidsalt;
            
        public:    // RAII
            ManagedMemoryTable();
            
        public:    // Methods
            
            /**
             * Assumes the given pointer points to valid managed memory, adds it
             * to the table, and returns a managed memory pointer.
             */
            ManagedMemoryPointerBase addPointer(ManagedMemoryOverhead* addr);
            
            /**
             * Templated version of addPointer which returns a templated
             * ManagedMemoryPointer wrapping the same type, for convenience.
             */
            template<typename T>
            ManagedMemoryPointer<T> addCastPointer(ManagedMemoryOverhead* addr) {
                return addPointer(addr);
            }
            
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
            T* get(const ManagedMemoryPointer<T>& ptr) const {
                return reinterpret_cast<T*>(get((const ManagedMemoryPointerBase)ptr));
            }
            
            void collect(StandardHashSet<ManagedMemoryPointerBase>& pointers) const;
            
        public:    // Management
            /**
             * Get the number of pages in this table.
             */
            uint32 countPages() const {
                return pages.length();
            }
            
            uint32 countRecordsEstimate() const;
            
            /**
             * Detects gaps in the table that were created upon removing a record.
             */
            void findGaps(uint32 minGapSize = 1);
            
            
        public:    // Iterator
            Iterator begin()  const { return Iterator(this, 0); }
            Iterator cbegin() const { return Iterator(this, 0); }
            Iterator end()    const { return Iterator(this, npos); }
            Iterator cend()   const { return Iterator(this, npos); }
            
            
        protected: // Methods
            static void decomposeTableIndex(uint32 tableIndex, uint32& pageIndex, uint32& recordIndex);
            
            ManagedMemoryTableRecord* getRecord(uint32 tableIndex) const;
            
            uint32 claimIndex();
        };
    }
}

#pragma warning(pop)
