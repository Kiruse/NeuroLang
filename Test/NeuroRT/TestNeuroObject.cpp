////////////////////////////////////////////////////////////////////////////////
// Unit Test for NeuroObjects. Because this is not an integration test, the test
// creates a custom environment for the objects to use, allowing us to exactly
// predict what should happen.
// -----
// Copyright (c) Kiruse 2018 Germany
// License: GPL 3.0
#include "NeuroObject.hpp"
#include "CLInterface.hpp"

#include <cstdlib>

using namespace Neuro;
using namespace Neuro::Runtime;


constexpr uint32 propCount = 4;


/**
 * Specialized derived pointer that does not use managed memory. Obviously for
 * testing purposes.
 */
class SpecialTestPointer : public ManagedMemoryPointerBase {
    uint8* pointer;
    
public:
    SpecialTestPointer(uint8* ptr) : pointer(ptr) {}
    
    void* get(uint32 index = 0) const {
        return pointer;
    }
    
    bool operator==(const SpecialTestPointer& other) const {
        return pointer == other.pointer;
    }
    bool operator!=(const SpecialTestPointer& other) const {
        return pointer != other.pointer;
    }
    
    operator bool() const {
        return !!pointer;
    }
};


uint8* gpBufferMain  = nullptr;
uint8* gpBufferOther = nullptr;
uint32 gCursor = 0;
uint32 gCursorOther = 0;


void swapBuffer() {
    uint8* tmpBuffer = gpBufferMain;
    gpBufferMain = gpBufferOther;
    gpBufferOther = tmpBuffer;
    
    uint32 tmpCursor = gCursor;
    gCursor = gCursorOther;
    gCursorOther = tmpCursor;
}

ManagedMemoryPointerBase allocateObject(uint32 bytes) {
    SpecialTestPointer pointer(gpBufferMain + gCursor);
    gCursor += bytes;
    return pointer;
}

Pointer createObject(uint32 knownPropsCount = propCount) {
    return Object::genericCreateObject(Object::AllocationDelegate::FunctionDelegate<allocateObject>(), propCount);
}

Pointer recreateObject(Pointer object, uint32 knownPropsCount) {
    return Object::genericRecreateObject(Object::AllocationDelegate::FunctionDelegate<allocateObject>(), object, knownPropsCount);
}


int main(int argc, char** argv)
{
    Object::setCreateObjectDelegate(Object::CreateObjectDelegate::FunctionDelegate<createObject>());
    Object::setRecreateObjectDelegate(Object::RecreateObjectDelegate::FunctionDelegate<recreateObject>());
    
    // 4kibi memory should be more than enough
    gpBufferMain  = (uint8*)std::malloc(4096);
    gpBufferOther = (uint8*)std::malloc(4096);
    
    Testing::section("NeuroObject", [](){
        Testing::test("Property Assignment (in range)", [](){
            
        });
        
        Testing::test("Property Map Overflow", [](){
            
        });
    });
}
