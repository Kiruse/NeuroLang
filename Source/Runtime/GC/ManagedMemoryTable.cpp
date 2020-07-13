////////////////////////////////////////////////////////////////////////////////
// Implementation of the managed memory table methods and everything that goes
// along with them.
// 
// Implementation assumes the pointers point to a
// Neuro::Runtime::ManagedMemoryOverhead prefixed buffer.
// 
// TODO: Remove empty pages
// -----
// Copyright (c) Kiruse 2018
// License: GPL 3.0

#include <cassert>

#include "Assert.hpp"
#include "HashCode.hpp"
#include "Concurrency/ScopeLocks.hpp"
#include "GC/ManagedMemoryTable.hpp"
#include "GC/ManagedMemoryOverhead.hpp"

namespace Neuro {
    namespace Runtime
    {
        using namespace Concurrency;
        
        ManagedMemoryTable::Iterator& ManagedMemoryTable::Iterator::operator++() {
            const uint32 maxIdx = table->pages.size() * NEURO_MANAGEDMEMORYTABLE_RECORDS_PER_PAGE;
            
            ManagedMemoryTableRecord* record;
            do {
                record = table->getRecord(++tableIndex);
            } while (record->ptr == nullptr && tableIndex < maxIdx);
            
            if (tableIndex >= maxIdx) {
                tableIndex = npos;
            }
            
            return *this;
        }
        
        ManagedMemoryTable::Iterator& ManagedMemoryTable::Iterator::operator--() {
            const uint32 maxIdx = table->pages.size() * NEURO_MANAGEDMEMORYTABLE_RECORDS_PER_PAGE;
            
            ManagedMemoryTableRecord* record;
            do {
                record = table->getRecord(++tableIndex);
            } while (record->ptr == nullptr && tableIndex < maxIdx);
            
            if (tableIndex >= maxIdx) {
                tableIndex = npos;
            }
            
            return *this;
        }
        

        ManagedMemoryTable::ManagedMemoryTable()
         : shouldExpand(false)
         , shouldScanGaps(false)
         , semaphore()
         , pages(10)
         , nextRecordIdx(0)
         , gapsSemaphore()
         , gaps(nullptr)
         , numGaps(0)
         , uidsalt(0)
        {
            pages.override_length(pages.size());
        }
        
        
        ManagedMemoryPointerBase ManagedMemoryTable::addPointer(ManagedMemoryOverhead* addr) {
            ManagedMemoryPointerBase result;
            
            uint32 tableIndex = claimIndex(), pageIndex, recordIndex;
            Assert::Value("Table index is valid", tableIndex != npos);
            decomposeTableIndex(tableIndex, pageIndex, recordIndex);
            
            // Double-checked lock
            if (pageIndex >= pages.size()) {
                UniqueLock lock(semaphore);
                if (pageIndex >= pages.size()) {
                    pages.resize(pages.size() + 10);
                    pages.override_length(pages.size());
                }
            }
            
            const hashT addrHash = calculateHash(addr);
            const hashT saltHash = calculateHash(uidsalt.fetch_add(1));
            const hashT uid = combineHashOrdered(addrHash, saltHash);
            
            {
                SharedLock lock(semaphore);
                ManagedMemoryTableRecord& record = *getRecord(tableIndex);
                record.ptr = addr;
                record.uid = uid;
            }
            
            result.tableIndex = tableIndex;
            result.rowuid = uid;
            return result;
        }
        
        Error ManagedMemoryTable::replacePointer(const ManagedMemoryPointerBase& ptr, ManagedMemoryOverhead* newAddr) {
            SharedLock lock(semaphore);
            
            ManagedMemoryTableRecord& record = *getRecord(ptr.tableIndex);
            if (record.uid != ptr.rowuid) return NotFoundError::instance();
            
            record.ptr = newAddr;
            return NoError::instance();
        }
        
        Error ManagedMemoryTable::removePointer(const ManagedMemoryPointerBase& ptr) {
            SharedLock lock(semaphore);
            
            ManagedMemoryTableRecord& record = *getRecord(ptr.tableIndex);
            if (record.uid != ptr.rowuid) return NotFoundError::instance();
            
            record.ptr = nullptr;
            record.uid = 0;
            shouldScanGaps = true;
            return NoError::instance();
        }
        
        void* ManagedMemoryTable::get(const ManagedMemoryPointerBase& ptr) const {
			auto& tmp = *getRecord(ptr.tableIndex);
            ManagedMemoryTableRecord& record = *getRecord(ptr.tableIndex);
            if (record.uid != ptr.rowuid) return nullptr;
            return reinterpret_cast<uint8*>(record.ptr) + sizeof(ManagedMemoryOverhead);
        }
        
        void ManagedMemoryTable::collect(StandardHashSet<ManagedMemoryPointerBase>& pointers) const {
            pointers.reserve(countRecordsEstimate());
            
            for (auto ptr : *this) {
                pointers.add(ptr);
            }
            
            pointers.shrink();
        }
        
        
        ////////////////////////////////////////////////////////////////////////
        // Management
        
        uint32 ManagedMemoryTable::countRecordsEstimate() const {
            uint32 max = nextRecordIdx.load();
            
            SharedLock lock(gapsSemaphore);
            for (uint32 gapIdx = 0; gapIdx < numGaps; ++gapIdx) {
                max = max - (gaps[gapIdx].end - gaps[gapIdx].start.load());
            }
            
            return max;
        }
        
        void ManagedMemoryTable::findGaps(uint32 minGapSize) {
            Assert::notYetImplemented();
        }
        
        
        ////////////////////////////////////////////////////////////////////////
        // Protected methods
        
        void ManagedMemoryTable::decomposeTableIndex(uint32 tableIndex, uint32& pageIndex, uint32& recordIndex) {
            pageIndex = tableIndex / NEURO_MANAGEDMEMORYTABLE_RECORDS_PER_PAGE;
            recordIndex = tableIndex % NEURO_MANAGEDMEMORYTABLE_RECORDS_PER_PAGE;
        }
        
        ManagedMemoryTableRecord* ManagedMemoryTable::getRecord(uint32 tableIndex) const {
            uint32 pageIndex, recordIndex;
            decomposeTableIndex(tableIndex, pageIndex, recordIndex);
            Assert::Value("Page and record indices valid", pages.size() > pageIndex && recordIndex < NEURO_MANAGEDMEMORYTABLE_RECORDS_PER_PAGE);
            auto* that = const_cast<ManagedMemoryTable*>(this);
            return &that->pages[pageIndex].records[recordIndex];
        }
        
        uint32 ManagedMemoryTable::claimIndex() {
            // Try to acquire soft lock on gaps semaphore. If unavailable, don't
            // wait and instead just append to end of table.
            TrySharedLock lock(gapsSemaphore);
            
            if (lock && gaps) {
                uint32 index = npos;
                for (uint32 i = 0; i < numGaps; ++i) {
                    index = gaps[i].claim();
                    if (index != npos) return index;
                }
            }
            
            return nextRecordIdx.fetch_add(1);
        }
    }
}