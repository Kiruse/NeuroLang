////////////////////////////////////////////////////////////////////////////////
// Unit Test for NeuroObjects. Because this is not an integration test, the test
// creates a custom environment for the objects to use, allowing us to exactly
// predict what should happen.
// -----
// NOTES
// -----
// It is not possible to test automatic object recreation independently from a
// ManagedMemoryTable as it would require proper ManagedMemoryPointerBase usage.
// Here, however, we store the offset of the pointer with respect to the
// beginning of the buffer, meaning every pointer is resolved to a static
// location.
// Instead of testing that manually, we know surrounding test cases should work,
// and thus infer that automatic recreation should work as well.
// -----
// Copyright (c) Kiruse 2018 Germany
// License: GPL 3.0
#include "GC/ManagedMemoryOverhead.hpp"
#include "GC/NeuroGC.hpp"
#include "NeuroObject.hpp"
#include "CLInterface.hpp"

#include <cstdlib>

using namespace Neuro;
using namespace Neuro::Runtime;


class FakeGC : public GCInterface
{
public: // Fields
    uint8* mainBuffer;
    uint8* otherBuffer;
    uint32 cursor;
    uint32 otherCursor;
    
public: // RAII
    FakeGC()
     : mainBuffer(new uint8[4096])
     , otherBuffer(new uint8[4096])
     , cursor(0)
     , otherCursor(0)
    {}
    
    FakeGC(const FakeGC&) = delete;
    FakeGC(FakeGC&&) = delete;
    FakeGC& operator=(const FakeGC&) = delete;
    FakeGC& operator=(FakeGC&&) = delete;
    
    ~FakeGC() {}
    
public: // GCInterface
    virtual ManagedMemoryPointerBase allocateTrivial(uint32 size, uint32 count) override {
        auto head = new (mainBuffer + cursor) ManagedMemoryOverhead(size, count);
        Pointer ptr = makePointer(cursor, 0);
        cursor += size * count;
        return ptr;
    }
    
    virtual ManagedMemoryPointerBase allocateNonTrivial(uint32 size, uint32 count, const Delegate<void, void*, const void*>& copyDeleg, const Delegate<void, void*>& destroyDeleg) override {
        auto head = new (mainBuffer + cursor) ManagedMemoryOverhead(size, count);
        copyDeleg.copyTo(&head->copyDelegate.get());
        destroyDeleg.copyTo(&head->destroyDelegate.get());
        Pointer pointer = makePointer(cursor, 0);
        cursor += size * count;
        return pointer;
    }
    
    virtual Error root(Pointer obj) override {
        // FakeGC does not actually manage memory, hence no roots
        return NoError::instance();
    }
    
    virtual Error unroot(Pointer obj) override {
        // FakeGC does not actually manage memory, hence no roots
        return NoError::instance();
    }
    
    virtual void* resolve(ManagedMemoryPointerBase pointer) override {
        uint32 index;
        hashT hash;
        extractPointerData(pointer, index, hash);
        return mainBuffer + index + sizeof(ManagedMemoryOverhead);
    }
    
public: // Methods
    void swapBuffer() {
        uint8* tmpBuffer = mainBuffer;
        mainBuffer = otherBuffer;
        otherBuffer = tmpBuffer;
        
        uint32 tmpCursor = cursor;
        cursor = otherCursor;
        otherCursor = tmpCursor;
    }
};


int main(int argc, char** argv)
{
    GC::init(new FakeGC());
    
    Testing::section("NeuroObject", [](){
        Testing::test("Get or Add Property", [](){
            Pointer obj = Object::createObject(4);
            
            auto val = obj->getProperty("foobar");
            Testing::assert(val.isUndefined(), "Property unexpectedly not unsigned");
            
            val = obj->getProperty("barfoo");
            Testing::assert(val.isUndefined(), "Property unexpectedly not unsigned");
            
            val = obj->getProperty("test");
            Testing::assert(val.isUndefined(), "Property unexpectedly not unsigned");
            
            val = obj->getProperty("testing");
            Testing::assert(val.isUndefined(), "Property unexpectedly not unsigned");
        });
        
        Testing::test("Property Assignment", [](){
            Pointer obj = Object::createObject(10);
            
            obj->getProperty("a") = 42;
            Testing::assert(obj->getProperty("a") == 42, "Failed to read/write integral property");
            
            obj->getProperty("b") = 42.f;
            Testing::assert(obj->getProperty("b") == 42., "Failed to read/write decimal property");
        });
        
        Testing::test("Manual Recreate", [](){
            Pointer oldObj = Object::createObject(4, 0);
            Testing::assert(oldObj->capacity() == 4, "Unexpected property map capacity");
            
            oldObj->getProperty("foobar")   =   42;
            oldObj->getProperty("barfoo")   =  420;
            oldObj->getProperty("testeroo") = 6969;
            
            Pointer newObj = Object::recreateObject(oldObj, 8, 0);
            Testing::assert(newObj->capacity() == 8, "Unexpected property map capacity");
            Testing::assert(newObj->length()   == 3, "Unexpected property map length");
            
            Testing::assert(newObj->getProperty("foobar")   ==   42, "Old property map not correctly copied");
            Testing::assert(newObj->getProperty("barfoo")   ==  420, "Old property map not correctly copied");
            Testing::assert(newObj->getProperty("testeroo") == 6969, "Old property map not correctly copied");
        });
    });
    
    GC::destroy();
}
