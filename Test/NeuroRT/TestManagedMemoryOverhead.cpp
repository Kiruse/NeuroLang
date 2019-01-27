////////////////////////////////////////////////////////////////////////////////
// Copyright (c) Kiruse 2018 Germany
// License: GPL 3.0
////////////////////////////////////////////////////////////////////////////////
#include <cstdlib>

#include "GC/ManagedMemoryOverhead.hpp"
#include "CLInterface.hpp"

using namespace Neuro;
using namespace Neuro::Runtime;

int main() {
    Testing::test("ManagedMemoryOverhead", [](){
        constexpr uint32 numBufferBytes = sizeof(uint32) * 10;
        
        uint8 buffer[numBufferBytes + sizeof(ManagedMemoryOverhead)];
        auto head = new (buffer) ManagedMemoryOverhead(sizeof(uint32), 10);
        
        Testing::assert(head->getBufferBytes() == numBufferBytes, "Wrong count of bytes in buffer");
        Testing::assert(head->getTotalBytes()  == numBufferBytes + sizeof(ManagedMemoryOverhead), "Wrong count of total bytes of memory region");
        
        Testing::assert(!head->copyDelegate.valid(), "Unexpected copy delegate");
        Testing::assert(!head->destroyDelegate.valid(), "Unexpected destroy delegate");
        
        Testing::assert((uint8*)head->getBufferPointer() == buffer + sizeof(ManagedMemoryOverhead), "Incorrect buffer start address");
        Testing::assert((uint8*)head->getBeyondPointer() == buffer + sizeof(ManagedMemoryOverhead) + numBufferBytes, "Incorrect beyond address");
    });
}
