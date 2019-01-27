////////////////////////////////////////////////////////////////////////////////
// Implementation of the managed memory table methods and everything that goes
// along with them.
// 
// Implementation assumes the pointers point to a
// Neuro::Runtime::ManagedMemoryOverhead prefixed buffer.
// -----
// Copyright (c) Kiruse 2018
// License: GPL 3.0

#include <cassert>

#include "HashCode.hpp"
#include "GC/ManagedMemoryTable.hpp"
#include "GC/ManagedMemoryOverhead.hpp"

namespace Neuro {
    namespace Runtime
    {
        constexpr uint32 g_pagesize = NEURO_MANAGEDMEMORYTABLEPAGE_SIZE;
        constexpr uint32 g_sectsize = NEURO_MANAGEDMEMORYTABLESECTION_SIZE;
        
        
        ManagedMemoryTable::ManagedMemoryTable() : firstPage(), uidsalt(0) {}
        
        
        ManagedMemoryPointerBase ManagedMemoryTable::addPointer(ManagedMemoryOverhead* addr) {
            // Non-changing comparison pointer value.
            static ManagedMemoryOverhead* cmpptr = nullptr;
            
            // Create the resulting abstract managed memory pointer.
            ManagedMemoryPointerBase result;
            
            // Find page and section (incl. indices) to insert into.
            ManagedMemoryTablePage* page;
            ManagedMemoryTableSection* section;
            uint32 pageIndex, sectionIndex;
            findSectionForInsert(addr, page, section, pageIndex, sectionIndex);
            assert(!!page && !!section);
            
            // Compute a likely locally unique ID from the address and an incrementing salt.
            const hashT addrHash = calculateHash(addr);
            const hashT saltHash = calculateHash(uidsalt.fetch_add(1));
            const hashT uid = combineHashOrdered(addrHash, saltHash);
            
            // Find the specific row and populate it with data.
            for (uint32 rowindex = 0; rowindex < g_sectsize; ++rowindex) {
                // Shortcut to the current row.
                auto& row = section->rows[rowindex];
                
                // Attempt to claim the row. A non-nullptr row is already claimed.
                if (row.ptr.compare_exchange_strong(cmpptr, addr)) {
                    row.uid = uid;
                    
                    // Populate the resulting managed memory pointer with valid data.
                    result.tableIndex = rowindex;
                    result.rowuid = uid;
                    break;
                }
            }
            
            return result;
        }
        
        Error ManagedMemoryTable::replacePointer(const ManagedMemoryPointerBase& ptr, ManagedMemoryOverhead* newAddr) {
            ManagedMemoryTablePage* page;
            ManagedMemoryTableSection* section;
            ManagedMemoryTableRow* row;
            
            // Find the corresponding row, if any.
            if (!get_internal(ptr.tableIndex, page, section, row)) return DataSetNotFoundError::instance();
            
            // Ensure the row exists & has the same rowuid.
            if (!row || row->uid != ptr.rowuid) return DataSetNotFoundError::instance();
            
            // Update the row.
            row->ptr = newAddr;
            return NoError::instance();
        }
        
        Error ManagedMemoryTable::removePointer(const ManagedMemoryPointerBase& ptr) {
            // Find the corresponding row, if any.
            ManagedMemoryTablePage* page;
            ManagedMemoryTableSection* section;
            ManagedMemoryTableRow* row;
            
            // Ensure the row exists & has the same rowuid.
            if (!get_internal(ptr.tableIndex, page, section, row)) {
                return DataSetNotFoundError::instance();
            }
            
            if (!row || row->uid != ptr.rowuid) return DataSetNotFoundError::instance();
            
            // Update the row.
            row->ptr = nullptr;
            row->uid = 0;
            
            // Decrement the section's row counter to allow other calls to find
            // the now free row in this section.
            section->numOccupied.fetch_sub(1);
            return NoError::instance();
        }
        
        void* ManagedMemoryTable::get(const ManagedMemoryPointerBase& ptr) const {
            ManagedMemoryTablePage* page;
            ManagedMemoryTableSection* section;
            ManagedMemoryTableRow* row;
            
            auto* that = const_cast<ManagedMemoryTable*>(this);
            
            if (!that->get_internal(ptr.tableIndex, page, section, row)) {
                return nullptr;
            }
            
            return reinterpret_cast<uint8*>(row->ptr.load()) + sizeof(ManagedMemoryOverhead);
        }
        
        Buffer<ManagedMemoryPointerBase> ManagedMemoryTable::getAllPointers() const {
            Buffer<ManagedMemoryPointerBase> result;
            const ManagedMemoryTablePage* page = &firstPage;
            
            uint32 tableIndex = 0;
            
            do {
                for (uint8 i = 0; i < sizeof(page->sections) / sizeof(ManagedMemoryTableSection); ++i) {
                    auto& section = page->sections[i];
                    
                    for (uint8 j = 0; j < sizeof(section.rows) / sizeof(ManagedMemoryTableRow); ++j) {
                        ManagedMemoryOverhead* ptr = section.rows[j].ptr.load();
                        if (ptr) {
                            ManagedMemoryPointerBase wrapper;
                            wrapper.tableIndex = tableIndex;
                            wrapper.rowuid = section.rows[j].uid;
                            result.add(wrapper);
                        }
                        
                        tableIndex++;
                    }
                }
            } while(page);
            
            return result;
        }
        
        
        ////////////////////////////////////////////////////////////////////////
        // Private methods
        
        void ManagedMemoryTable::findSectionForInsert(ManagedMemoryOverhead* addr, ManagedMemoryTablePage*& page, ManagedMemoryTableSection*& section, uint32& pageIndex, uint32& sectionIndex) {
            static ManagedMemoryTablePage* cmpptr = nullptr;
            
            // Attempt to find a section in an existing page.
            ManagedMemoryTablePage* lastPage = page = &firstPage;
            
            // Check if first page has any available slot
            sectionIndex = findSectionForInsert(page, addr);
            
            // Check if any other page has any available slot (if needed)
            while (sectionIndex == npos && page) {
                page = page->nextPage;
                sectionIndex = findSectionForInsert(page, addr);
            } while (sectionIndex == npos && page);
            
            // Still not found?! Try to create a new page! But also consider concurrency...
            while (sectionIndex == npos) {
                // Precheck: are we really the last page?
                // There may be a rare case where e.g. two new last pages were added in the meantime, but we still want to try both.
                if (!lastPage->nextPage) {
                    page = new ManagedMemoryTablePage();
                    page->sections[0].numOccupied = 1;
                    sectionIndex = npos;
                    section = &page->sections[0];
                }
                
                // Attempt to update the last page.
                if (!lastPage->nextPage.compare_exchange_strong(cmpptr, page)) {
                    // Another thread was faster than us.
                    // Update locally stored last page.
                    lastPage = lastPage->nextPage;
                    
                    // Delete our newly created page again, if precheck was true.
                    if (page) {
                        delete page;
                        page = lastPage;
                    }
                    
                    // Then try to find a section in the existing new page.
                    sectionIndex = findSectionForInsert(lastPage, addr);
                }
            }
            
            if (page) {
                // Return the section by pointer, for convenience.
                section = &page->sections[sectionIndex];
            }
            else {
                section = nullptr;
            }
        }
        
        uint32 ManagedMemoryTable::findSectionForInsert(ManagedMemoryTablePage* page, ManagedMemoryOverhead* newAddr) {
            for (uint32 sectionIndex = 0; sectionIndex < g_pagesize; ++sectionIndex) {
                auto& section = page->sections[sectionIndex];
                uint8 count = section.numOccupied.fetch_add(1);
                
                // Undo if more than g_sectsize entries
                if (count >= g_sectsize) {
                    section.numOccupied.fetch_sub(1);
                }
                
                // Having already incremented the counter, we basically already
                // reserved one row for us. Now we just gotta find it.
                else {
                    return sectionIndex;
                }
            }
            return npos;
        }
        
        bool ManagedMemoryTable::get_internal(uint32 index, ManagedMemoryTablePage*& page, ManagedMemoryTableSection*& section, ManagedMemoryTableRow*& row) {
            // Find the corresponding table page.
            page = &firstPage;
            
            while (page && index > g_pagesize * g_sectsize) {
                page = page->nextPage;
                index -= g_pagesize * g_sectsize;
            }
            
            if (!page) return false;
            
            // Locate the row.
            uint32 sectionIndex = index / g_sectsize;
            index %= g_sectsize;
            
            if (sectionIndex > g_pagesize) return false;
            
            section = &page->sections[sectionIndex];
            row = &section->rows[index];
            return true;
        }
    }
}