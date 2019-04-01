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
#include "NeuroBuffer.hpp"
#include "NeuroObject.hpp"
#include "CLInterface.hpp"
#include "GC/ManagedMemoryOverhead.hpp"
#include "GC/NeuroGC.hpp"

#include <cstdlib>
#include <cstring>
#include <utility>

using namespace Neuro;
using namespace Neuro::Runtime;


struct FakeGCRecord
{
    uint8* addr;
    hashT hash;
    
    FakeGCRecord() = default;
    FakeGCRecord(uint8* addr, hashT hash) : addr(addr), hash(hash) {}
};

class FakeGC : public GCInterface
{
public: // Fields
    Buffer<FakeGCRecord> records;
    
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
        uint8* buffer = mainBuffer + cursor;
        hashT hash = calculateHash(buffer);
        
        FakeGCRecord record(buffer, hash);
        auto* head = new (buffer) ManagedMemoryOverhead(size, count);
		head->isTrivial = true;
        
        Pointer ptr = makePointer(records.length(), hash);
        records.add(record);
        
        cursor += size * count;
        return ptr;
    }
    
    virtual ManagedMemoryPointerBase allocateNonTrivial(uint32 size, uint32 count, const Delegate<void, void*, const void*>& copyDeleg, const Delegate<void, void*>& destroyDeleg) override {
        uint8* buffer = mainBuffer + cursor;
        hashT hash = calculateHash(buffer);
        
        FakeGCRecord record(buffer, calculateHash(buffer));
        auto* head = new (buffer) ManagedMemoryOverhead(size, count);
		head->isTrivial = false;
        copyDeleg.copyTo(&head->copyDelegate.get());
        destroyDeleg.copyTo(&head->destroyDelegate.get());
        
        Pointer pointer = makePointer(records.length(), hash);
        records.add(record);
        
        cursor += size * count;
        return pointer;
    }
    
    virtual Error reallocate(ManagedMemoryPointerBase ptr, uint32 size, uint32 count, bool autocopy = true) {
        ManagedMemoryOverhead* oldHead = GC::getOverhead(ptr);
        uint32 tableIndex;
        hashT hash;
        extractPointerData(ptr, tableIndex, hash);
        
        auto& record = records[tableIndex];
        if (record.hash != hash) {
            Assert::shouldNotEnter();
            return GenericError::instance();
        }
        
        uint8* oldBuffer = record.addr;
        uint8* newBuffer = mainBuffer + cursor;
        cursor += size * count;
        
        ManagedMemoryOverhead* newHead = new (newBuffer) ManagedMemoryOverhead(size, count);
        newHead->isTrivial = oldHead->isTrivial;
        
        if (oldHead->isTrivial) {
            std::memset(newBuffer, 0, size * count);
            if (autocopy) {
                std::memcpy(newBuffer, oldBuffer, std::min(oldHead->elementSize * oldHead->count, size * count));
            }
        }
        else {
            oldHead->copyDelegate.get().copyTo(&newHead->copyDelegate.get());
            oldHead->destroyDelegate.get().copyTo(&newHead->destroyDelegate.get());
            
            if (autocopy) {
                newHead->copyDelegate.get()(newHead->getBufferPointer(), oldHead->getBufferPointer());
            }
        }
        
        record.addr = newBuffer;
        return NoError::instance();
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
        
        auto& record = records[index];
        if (record.hash != hash) return nullptr;
        return record.addr + sizeof(ManagedMemoryOverhead);
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
