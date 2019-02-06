////////////////////////////////////////////////////////////////////////////////
// Copyright (c) Kiruse 2018 Germany
// License: GPL 3.0
////////////////////////////////////////////////////////////////////////////////
#include <cstdlib>

#include "CLInterface.hpp"
#include "GC/ManagedMemoryTable.hpp"
#include "NeuroBuffer.hpp"

using namespace Neuro;
using namespace Neuro::Runtime;

ManagedMemoryTable gTable;
uint8* gpMainBuffer;
uint8* gpBufferOther;
uint32 gCursor = 0;
uint32 gCursorOther;


template<typename T>
ManagedMemoryOverhead* allocateBuffer(uint32 count) {
    auto head = new (gpMainBuffer + gCursor) ManagedMemoryOverhead(sizeof(T), count);
    gCursor += head->getTotalBytes();
    return head;
}

void swapBuffer() {
    uint8* tmpBuffer = gpMainBuffer;
    gpMainBuffer = gpBufferOther;
    gpBufferOther = tmpBuffer;
    
    uint32 tmpCursor = gCursor;
    gCursor = gCursorOther;
    gCursorOther = tmpCursor;
}


int main() {
    ManagedMemoryPointerBase::useOverheadLookupTable(&gTable);
    
    gpMainBuffer = (uint8*)std::malloc(1024);
    gpBufferOther = (uint8*)std::malloc(1024);
    
    Testing::section("ManagedMemoryTable", [&]() {
        Testing::test("Insert", [&]() {
            auto head = allocateBuffer<int32>(1);
            auto ptr = gTable.addCastPointer<int32>(head);
            Testing::assert(ptr.get() == head->getBufferPointer(), "Failed to properly insert into table");
        });
        
        Testing::test("Replace", [&]() {
            constexpr int32 count = 4;
            auto head = allocateBuffer<int32>(count);
            auto ptr = gTable.addCastPointer<int32>(head);
            Buffer<void*> dataPointers(count); // Pointers to data in first buffer for comparison later
            
            // Populate first buffer with data
            for (uint32 i = 0; i < count; ++i) {
                ptr[i] = i;
                dataPointers.add(ptr.get(i));
            }
            
            for (uint32 i = 0; i < count; ++i) {
                Testing::assert(*((uint8*)head->getBufferPointer() + sizeof(int32) * i) == i, "Failed to write value");
            }
            
            // Migrate to buffer 2
            
            swapBuffer();
            
            head = allocateBuffer<int32>(count);
            gTable.replacePointer(ptr, head);
            
            for (uint32 i = 0; i < count; ++i) {
                ptr[i] = i + 2;
            }
            
            for (uint32 i = 0; i < count; ++i) {
                Testing::assert(*((uint8*)head->getBufferPointer() + sizeof(int32) * i) == i + 2, "Failed to overwrite value");
                Testing::assert(ptr.get() != dataPointers[i], "Table pointing to old data");
            }
            
            // Revert to original buffer
            
            swapBuffer();
        });
        
        Testing::test("Remove", [&]() {
            auto head = allocateBuffer<int32>(1);
            auto ptr = gTable.addCastPointer<int32>(head);
            
            Testing::assert(ptr.get() == head->getBufferPointer(), "Failed to insert into table (prerequisite)");
            
            gTable.removePointer(ptr);
            
            Testing::assert(ptr.get() == nullptr, "Failed to remove from table");
        });
    });
}
