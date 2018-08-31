////////////////////////////////////////////////////////////////////////////////
// Unit Test of the Neuro::Buffer template class.
// -----
// Copyright (c) Kiruse 2018 Germany
#include <iostream>
#include "Assert.hpp"
#include "NeuroBuffer.hpp"
#include "NeuroTestingFramework.hpp"


////////////////////////////////////////////////////////////////////////////////
// Custom operators defined for the purpose of this unit test only.

namespace Neuro {
    template<typename T>
    bool operator==(const Buffer<T>& lhs, const Buffer<T>& rhs) {
        if (lhs.length() != rhs.length()) return false;
        for (size_t i = 0; i < lhs.length(); ++i) {
            if (lhs[i] != rhs[i]) return false;
        }
        return true;
    }
    
    template<typename T>
    bool operator!=(const Buffer<T>& lhs, const Buffer<T>& rhs) {
        return !(lhs == rhs);
    }
    
    template<typename T>
    std::ostream& operator<<(std::ostream& lhs, const Buffer<T>& rhs) {
        lhs << "[";
        if (rhs.length()) {
            if (Neuro::StringBuilder::canFormat(rhs[0])) {
                Neuro::StringBuilder builder;
                for (size_t i = 0; i < rhs.length(); ++i) {
                    builder << rhs[i];
                    if (i != rhs.length() - 1) builder << ",";
                }
                lhs << builder;
            }
            else {
                lhs << "some value";
                if (rhs.length() > 1) lhs << "s";
            }
        }
        return lhs << "]";
    }
}

/**
 * Non-trivial test case for the Neuro::Buffer container. Should use the RAIIHeapAllocator,
 * which requires the data type to be default constructible.
 */
class TestClass {
public:
    int value;
    
    // Non-trivial default constructor renders the entire class non-trivial.
    TestClass(int value = -1) : value(value) {}
    
    operator int() const { return value; }
};


////////////////////////////////////////////////////////////////////////////////
// The actual unit test definition.

void TestNeuroBuffer() {
    using namespace std;
    using namespace Neuro;
    using namespace Neuro::Testing;
    
    section("Neuro::Buffer (Trivial)", [](){
        test("Constructor w/ initializer list", [](){
            Buffer<int> buffer({1, 2, 3, 4}, 12);
            NEURO_ASSERT_EXPR(buffer.length()) == 4;
            NEURO_ASSERT_EXPR(buffer[0]) == 1;
            NEURO_ASSERT_EXPR(buffer[1]) == 2;
            NEURO_ASSERT_EXPR(buffer[2]) == 3;
            NEURO_ASSERT_EXPR(buffer[3]) == 4;
        });
        
        test("Copy Constructor", [](){
            Buffer<int> ref({1, 2, 3});
            Buffer<int> buffer(ref);
            Assert::Value(buffer) == ref;
        });
        
        test("Assignment w/ initializer list", [](){
            Buffer<int> buffer, ref {1, 2, 3, 4};
            buffer = {1, 2, 3, 4};
            Assert::Value(buffer) == ref;
        });
        
        test("Copy Assignment", [](){
            Buffer<int> buffer, ref({1, 2, 3});
            buffer = ref;
            Assert::Value(buffer) == ref;
        });
        
        test("Resizing", [](){
            Assert::Value test(Buffer<int>({1, 2, 3}, 10)), ref(Buffer<int>({1, 2, 3}));
            auto& buffer = test.value;
            
            buffer.resize(200);
            test == ref;
            NEURO_ASSERT_EXPR(buffer.size()) == 200;
            
            buffer.fit(15);
            test == ref;
            NEURO_ASSERT_EXPR(buffer.size()) == 20;
            
            buffer.shrink();
            test == ref;
            NEURO_ASSERT_EXPR(buffer.size()) == 3;
        });
        
        test("Add", [](){
            Buffer<int> other {1, 2, 3}, ref {};
            Assert::Value test("buffer", Buffer<int>{});
            auto& buffer = test.value;
            
            buffer.add(42);
            ref = {42};
            test == ref;
            
            buffer.add({1, 2, 3});
            ref = {42, 1, 2, 3};
            test == ref;
            
            buffer.add(other.begin(), other.end());
            ref = {42, 1, 2, 3, 1, 2, 3};
            test == ref;
            
            buffer.add(other);
            ref = {42, 1, 2, 3, 1, 2, 3, 1, 2, 3};
            test == ref;
        });
        
        test("Insert", [](){
            Buffer<int> other({1, 2, 3}), ref{};
            Assert::Value test(Buffer<int>{24, 25, 69});
            auto& buffer = test.value;
            
            buffer.insert(2, 42);
            ref = {24, 25, 42, 69};
            test == ref;
            
            buffer.insert(1, {1, 2, 3});
            ref = {24, 1, 2, 3, 25, 42, 69};
            test == ref;
            
            buffer.insert(2, other);
            ref = {24, 1, 1, 2, 3, 2, 3, 25, 42, 69};
            test == ref;
        });
        
        test("Get Reference", [](){
            Buffer<int> buffer({1, 2, 3});
            buffer[1] = 69;
            NEURO_ASSERT_EXPR(buffer[1]) == 69;
        });
        
        test("Drop", [](){
            Buffer<int> buffer {1, 2, 3, 4}, ref {1, 2, 3};
            buffer.drop();
            Assert::Value(buffer) == ref;
        });
        
        test("Merge", [](){
            Buffer<int> buffer {1, 2, 3}, other {3, 4}, ref {1, 2, 3, 3, 4};
            buffer.merge(other);
            Assert::Value(buffer) == ref;
        });
        
        test("Splice", [](){
            Buffer<int> buffer {1, 2, 3, 4}, ref {1, 4};
            buffer.splice(1, 2);
            Assert::Value(buffer) == ref;
        });
        
        test("Clear", [](){
            Buffer<int> buffer({1, 2, 3, 4, 5});
            buffer.clear();
            NEURO_ASSERT_EXPR(buffer.length()) == 0;
        });
    });
    
    section("Neuro::Buffer (Non-Trivial)", [](){
        typedef Buffer<TestClass> Buff;
        
        test("Constructor w/ initializer list", [](){
            Buff buffer({TestClass(1), TestClass(2)}, 12);
            NEURO_ASSERT_EXPR(buffer.length()) == 2;
            NEURO_ASSERT_EXPR(buffer[0].value) == 1;
            NEURO_ASSERT_EXPR(buffer[1].value) == 2;
        });
        
        test("Copy Constructor", [](){
            Buff ref({1, 2, 3});
            Buff buffer(ref);
            Assert::Value(buffer) == ref;
        });
        
        test("Assignment w/ initializer list", [](){
            Buff buffer, ref {1, 2, 3, 4};
            buffer = {1, 2, 3, 4};
            Assert::Value(buffer) == ref;
        });
        
        test("Copy Assignment", [](){
            Buff buffer, ref({1, 2, 3});
            buffer = ref;
            Assert::Value(buffer) == ref;
        });
        
        test("Resizing", [](){
            Assert::Value test(Buff({1, 2, 3}, 10)), ref(Buff({1, 2, 3}));
            auto& buffer = test.value;
            
            buffer.resize(200);
            test == ref;
            NEURO_ASSERT_EXPR(buffer.size()) == 200;
            
            buffer.fit(15);
            test == ref;
            NEURO_ASSERT_EXPR(buffer.size()) == 20;
            
            buffer.shrink();
            test == ref;
            NEURO_ASSERT_EXPR(buffer.size()) == 3;
        });
        
        test("Add", [](){
            Buff other {1, 2, 3}, ref {};
            Assert::Value test("buffer", Buff{});
            auto& buffer = test.value;
            
            buffer.add(TestClass(42));
            ref = {42};
            test == ref;
            
            buffer.add({1, 2, 3});
            ref = {42, 1, 2, 3};
            test == ref;
            
            buffer.add(other.begin(), other.end());
            ref = {42, 1, 2, 3, 1, 2, 3};
            test == ref;
            
            buffer.add(other);
            ref = {42, 1, 2, 3, 1, 2, 3, 1, 2, 3};
            test == ref;
        });
        
        test("Insert", [](){
            Buff other({1, 2, 3}), ref{};
            Assert::Value test(Buff{24, 25, 69});
            auto& buffer = test.value;
            
            buffer.insert(2, TestClass(42));
            ref = {24, 25, 42, 69};
            test == ref;
            
            buffer.insert(1, {1, 2, 3});
            ref = {24, 1, 2, 3, 25, 42, 69};
            test == ref;
            
            buffer.insert(2, other);
            ref = {24, 1, 1, 2, 3, 2, 3, 25, 42, 69};
            test == ref;
        });
        
        test("Get Reference", [](){
            Buff buffer({1, 2, 3});
            buffer[1] = 69;
            NEURO_ASSERT_EXPR(buffer[1]) == 69;
        });
        
        test("Drop", [](){
            Buff buffer {1, 2, 3, 4}, ref {1, 2, 3};
            buffer.drop();
            Assert::Value(buffer) == ref;
        });
        
        test("Merge", [](){
            Buff buffer {1, 2, 3}, other {3, 4}, ref {1, 2, 3, 3, 4};
            buffer.merge(other);
            Assert::Value(buffer) == ref;
        });
        
        test("Splice", [](){
            Buff buffer {1, 2, 3, 4}, ref {1, 4};
            buffer.splice(1, 2);
            Assert::Value(buffer) == ref;
        });
        
        test("Clear", [](){
            Buff buffer({1, 2, 3, 4, 5});
            buffer.clear();
            NEURO_ASSERT_EXPR(buffer.length()) == 0;
        });
    });
}
