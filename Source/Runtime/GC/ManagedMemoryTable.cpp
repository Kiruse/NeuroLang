////////////////////////////////////////////////////////////////////////////////
// Implementation of the managed memory table methods and everything that goes
// along with them.
// -----
// Copyright (c) Kiruse 2018
// License: GPL 3.0

#include <cassert>

#include "ManagedMemoryTable.hpp"

namespace Neuro {
    namespace Runtime
    {
        constexpr uint32 g_pagesize = NEURO_MANAGEDMEMORYTABLEPAGE_SIZE;
        constexpr uint32 g_sectsize = NEURO_MANAGEDMEMORYTABLESECTION_SIZE;
        
        
        ManagedMemoryTable::ManagedMemoryTable() : firstPage(), uidsalt(0) {}
        
        
        ManagedMemoryPointerBase ManagedMemoryTable::addPointer(void* addr) {
            // Non-changing comparison pointer value.
            static constexpr void* cmpptr = nullptr;
            
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
        
        Error ManagedMemoryTable::replacePointer(const ManagedMemoryPointerBase& ptr, void* newAddr) {
            // Find the corresponding row, if any.
            auto* row = get_internal(ptr.tableIndex);
            
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
            if (!get_internal(ptr.tableIndex, page, section, row)) {
                return nullptr;
            }
            return row->ptr;
        }
        
        
        ////////////////////////////////////////////////////////////////////////
        // Private methods
        
        void ManagedMemoryTable::findSectionForInsert(void* addr, ManagedMemoryTablePage*& page, ManagedMemoryTableSection*& section, uint32& pageIndex, uint32& sectionIndex) {
            static constexpr ManagedMemoryTablePage* cmpptr = nullptr;
            
            // Attempt to find a section in an existing page.
            ManagedMemoryTablePage* lastPage = page = &firstPage;
            do {
                sectionIndex = findSectionForInsert(page, addr);
                page = page->nextPage;
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
                    sectionIndex = findSectionForInsert(lastPage);
                }
            }
            
            // Return the section by pointer, for convenience.
            section = &page->sections[sectionIndex];
        }
        
        uint32 ManagedMemoryTable::findSectionForInsert(ManagedMemoryTablePage* page, void* newAddr) {
            for (sectionIndex = 0; sectionIndex < g_pagesize; ++sectionIndex) {
                section = page->sections[sectionIndex];
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
            
            section = &page.sections[sectionIndex];
            row = &section->rows[index];
            return true;
        }
    }
}