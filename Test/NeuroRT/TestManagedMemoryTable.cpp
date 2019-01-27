////////////////////////////////////////////////////////////////////////////////
// Copyright (c) Kiruse 2018 Germany
// License: GPL 3.0
////////////////////////////////////////////////////////////////////////////////
#include <cstdlib>

#include "GC/ManagedMemoryTable.hpp"
#include "CLInterface.hpp"

using namespace Neuro;
using namespace Neuro::Runtime;

ManagedMemoryTable gTable;
uint8* gpMainBuffer;
uint8* gpBuffer1;
uint8* gpBuffer2;
uint32 gCursor = 0;


template<typename T>
ManagedMemoryOverhead* allocateBuffer(uint32 count) {
    auto head = new (gpMainBuffer + gCursor) ManagedMemoryOverhead(sizeof(T), count);
    gCursor += head->getTotalBytes();
    return head;
}


int main() {
    ManagedMemoryPointerBase::useOverheadLookupTable(&gTable);
    
    gpMainBuffer = gpBuffer1 = (uint8*)std::malloc(1024);
    gpBuffer2 = (uint8*)std::malloc(1024);
    
    ManagedMemoryOverhead* headScalar,
                         * headSmallArray,
                         * headLargeArray,
                         * headVector3f,
                         * headVector4d;
    
    ManagedMemoryPointer<int32>  ptrScalar;
    ManagedMemoryPointer<int32>  ptrSmallArray;
    ManagedMemoryPointer<int32>  ptrLargeArray;
    ManagedMemoryPointer<float>  ptrVector3f;
    ManagedMemoryPointer<double> ptrVector4d;
    
    Testing::section("ManagedMemoryTable", [&]() {
        Testing::test("Scalar int", [&]() {
            headScalar = allocateBuffer<int32>(1);
            ptrScalar = gTable.addCastPointer<int32>(headScalar);
            *ptrScalar = 42;
            
            Testing::assert(*((int32*)headScalar->getBufferPointer()) == 42, "Failed to write scalar int32");
        });
        
        Testing::test("10-int array", [&]() {
            headSmallArray = allocateBuffer<int32>(10);
            ptrSmallArray = gTable.addCastPointer<int32>(headSmallArray);
            
            for (uint32 i = 0; i < 10; ++i) {
                ptrSmallArray[i] = i;
            }
            
            uint32* ptr = (uint32*)headSmallArray->getBufferPointer();
            for (uint32 i = 0; i < 10; ++i) {
                Testing::assert(ptr[i] == i, "Failed to write to small int32 array");
            }
        });
        
        Testing::test("42-int array", [&]() {
            headLargeArray = allocateBuffer<int32>(42);
            ptrLargeArray = gTable.addCastPointer<int32>(headLargeArray);
            
            for (uint32 i = 0; i < 42; ++i) {
                ptrLargeArray[i] = i;
            }
            
            uint32* ptr = (uint32*)headLargeArray->getBufferPointer();
            for (uint32 i = 0; i < 42; ++i) {
                Testing::assert(ptr[i] == i, "Failed to write to large int32 array");
            }
        });
        
        Testing::test("3-float vector", [&]() {
            headVector3f = allocateBuffer<float>(3);
            ptrVector3f = gTable.addCastPointer<float>(headVector3f);
            
            ptrVector3f[0] = 1;
            ptrVector3f[1] = 2;
            ptrVector3f[2] = 3;
            
            float* ptr = (float*)headVector3f->getBufferPointer();
            Testing::assert(ptr[0] == 1, "Failed to write to vector3f");
            Testing::assert(ptr[1] == 2, "Failed to write to vector3f");
            Testing::assert(ptr[2] == 3, "Failed to write to vector3f");
        });
        
        Testing::test("4-double vector", [&]() {
            headVector4d = allocateBuffer<double>(4);
            ptrVector4d = gTable.addCastPointer<double>(headVector4d);
            
            ptrVector4d[0] = 1;
            ptrVector4d[1] = 2;
            ptrVector4d[2] = 3;
            ptrVector4d[3] = 4;
            
            double* ptr = (double*)headVector4d->getBufferPointer();
            Testing::assert(ptr[0] == 1, "Failed to write to vector4d");
            Testing::assert(ptr[1] == 2, "Failed to write to vector4d");
            Testing::assert(ptr[2] == 3, "Failed to write to vector4d");
            Testing::assert(ptr[3] == 4, "Failed to write to vector4d");
        });
    });
}
