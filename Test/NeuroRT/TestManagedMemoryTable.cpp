////////////////////////////////////////////////////////////////////////////////
// Copyright (c) Kiruse 2018 Germany
// License: GPL 3.0
////////////////////////////////////////////////////////////////////////////////
#include <cstdlib>

#include "Assert.hpp"
#include "CLInterface.hpp"
#include "NeuroBuffer.hpp"
#include "GC/ManagedMemoryTable.hpp"
#include "GC/NeuroGC.hpp"

using namespace Neuro;
using namespace Neuro::Runtime;


class FakeGC : public GCInterface {
public:
    ManagedMemoryTable dataTable;
    uint8* mainBuffer;
    uint8* otherBuffer;
    uint32 cursor;
    uint32 otherCursor;
    
public: // RAII
    FakeGC()
     : dataTable()
     , mainBuffer(new uint8[1024])
     , otherBuffer(new uint8[1024])
     , cursor(0)
     , otherCursor(0)
    {}
    
    FakeGC(const FakeGC&) = delete;
    FakeGC(FakeGC&&) = delete;
    FakeGC& operator=(const FakeGC&) = delete;
    FakeGC& operator=(FakeGC&&) = delete;
    
    ~FakeGC() {
        delete mainBuffer;
        delete otherBuffer;
    }
    
    
public: // GCInterface
    virtual ManagedMemoryPointerBase allocateTrivial(uint32 elementSize, uint32 count) override {
        auto head = new (mainBuffer + cursor) ManagedMemoryOverhead(elementSize, count);
        cursor += head->getTotalBytes();
        return dataTable.addPointer(head);
    }
    
    virtual ManagedMemoryPointerBase allocateNonTrivial(uint32 elementSize, uint32 count, const Delegate<void, void*, const void*>& copyDeleg, const Delegate<void, void*>& destroyDeleg) override {
        auto head = new (mainBuffer + cursor) ManagedMemoryOverhead(elementSize, count);
        copyDeleg.copyTo(&head->copyDelegate.get());
        destroyDeleg.copyTo(&head->destroyDelegate.get());
        cursor += head->getTotalBytes();
        return dataTable.addPointer(head);
    }
    
    virtual Error reallocate(ManagedMemoryPointerBase ptr, uint32 size, uint32 count, bool autocopy = true) override {
        Assert::shouldNotEnter();
        return GenericError::instance();
    }
    
    virtual Error root(Pointer obj) override {
        // Fake GC doesn't scan, so no roots
        return NoError::instance();
    }
    
    virtual Error unroot(Pointer obj) override {
        // Fake GC doesn't scan, so no roots
        return NoError::instance();
    }
    
    virtual void* resolve(ManagedMemoryPointerBase pointer) override {
        return dataTable.get(pointer);
    }

public: // Methods
    template<typename T>
    ManagedMemoryPointer<T> alloc(uint32 count = 1) {
        return allocateTrivial(sizeof(T), count);
    }
    
    void swapBuffer() {
        uint8* tmpBuffer = mainBuffer;
        mainBuffer = otherBuffer;
        otherBuffer = tmpBuffer;
        
        uint32 tmpCursor = cursor;
        cursor = otherCursor;
        otherCursor = tmpCursor;
    }
    
    void updatePointer(ManagedMemoryPointerBase updateMe, ManagedMemoryPointerBase updateWith) {
        dataTable.replacePointer(updateMe, GC::getOverhead(updateWith));
    }
    
    void removePointer(ManagedMemoryPointerBase ptr) {
        dataTable.removePointer(ptr);
    }
    
    ManagedMemoryOverhead* getCursorPointer() const {
        return reinterpret_cast<ManagedMemoryOverhead*>(mainBuffer + cursor);
    }
};


int main() {
    auto gc = new FakeGC();
    GC::init(gc);
    
    Testing::section("ManagedMemoryTable", [&]() {
        Testing::test("Insert", [&]() {
            auto head = gc->getCursorPointer();
            auto ptr = gc->alloc<int32>();
            Testing::assert((void*)ptr.get() == head->getBufferPointer(), "Failed to properly insert into table");
        });
        
        Testing::test("Replace", [&]() {
            constexpr int32 count = 4;
            
            auto oldHead = gc->getCursorPointer();
            auto ptr = gc->alloc<int32>(count);
            
            // Populate first buffer with data
            for (uint32 i = 0; i < count; ++i) {
                ptr[i] = i;
            }
            
            for (uint32 i = 0; i < count; ++i) {
                Testing::assert(((int32*)oldHead->getBufferPointer())[i] == i, "Failed to write value");
            }
            
            // Migrate to off-buffer
            
            gc->swapBuffer();
            auto newHead = gc->getCursorPointer();
            gc->updatePointer(ptr, gc->alloc<int32>(count));
            
            Testing::assert(oldHead != newHead, "Old buffer is not supposed to be the same as new buffer");
            
            for (uint32 i = 0; i < count; ++i) {
                ptr[i] = i + 2;
            }
            
            for (uint32 i = 0; i < count; ++i) {
                Testing::assert(*ptr.get(i) == i + 2, "Failed to overwrite value");
            }
            
            // Revert to original buffer
            
            gc->swapBuffer();
        });
        
        Testing::test("Remove", [&]() {
            auto head = gc->getCursorPointer();
            auto ptr = gc->alloc<int32>();
            
            Testing::assert(ptr.get() == head->getBufferPointer(), "Failed to insert into table (prerequisite)");
            
            gc->removePointer(ptr);
            
            Testing::assert(ptr.get() == nullptr, "Failed to remove from table");
        });
    });
    
    GC::destroy();
}
