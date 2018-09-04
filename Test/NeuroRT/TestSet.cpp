////////////////////////////////////////////////////////////////////////////////
// Unit Test of the Neuro Hash Set implementations.
// -----
// Copyright (c) Kiruse 2018
// License: GPL 3.0
#include <iostream>

#include "Assert.hpp"
#include "NeuroSet.hpp"
#include "NeuroTestingFramework.hpp"

namespace Neuro
{
    template<typename T, bool (*Comparator)(const T&, const T&), uint32 (*Hasher)(const T&), typename BucketT, typename Alloc>
    std::ostream& operator<<(std::ostream& lhs, const StandardHashSet<T, Comparator, Hasher, BucketT, Alloc>& rhs) {
        lhs << "{";
        if (StringBuilder::canFormat(*rhs.begin())) {
            StringBuilder builder;
            uint32 total = rhs.count(), curr = 0;
            for (auto elem : rhs) {
                builder << elem;
                if (++curr != total) builder << ", ";
            }
            lhs << builder;
        }
        else {
            if (const uint32 count = rhs.count()) {
                lhs << "some value";
                if (count > 1) lhs << 's';
            }
        }
        return lhs << "}";
    }
}

void TestNeuroSet() {
    using namespace std;
    using namespace Neuro;
    using namespace Neuro::Testing;
    
    section("Neuro Standard Hash Set", [](){
        test("Basic Construction", [](){
            StandardHashSet<int> set(8);
            NEURO_ASSERT_EXPR(set.count()) == 0;
            NEURO_ASSERT_EXPR(set.capacity()) == 0;
        });
        
        test("Basic Add", [](){
            StandardHashSet<int> set(8);
            for (int i = 0; i < 16; ++i) {
                set.add(i);
            }
            
            NEURO_ASSERT_EXPR(set.count()) == 16;
            int expected = 0;
            for (auto curr : set) {
                Assert::Value(curr) == expected++;
            }
        });
        
        test("Assignment", [](){
            StandardHashSet<int> set = {1, 2, 3};
            NEURO_ASSERT_EXPR(set.count()) == 3;
            int expected = 1;
            for (auto curr : set) {
                Assert::Value(curr) == expected++;
            }
            
            StandardHashSet<int> set2 = set;
            Assert::Value(set) == set2;
        });
        
        test("Clear", [](){
            StandardHashSet<int> set {1, 2, 3, 4};
            set.clear();
            NEURO_ASSERT_EXPR(set.count()) == 0;
        });
        
        test("Add", [](){
            StandardHashSet<int> set, other {1, 2, 3}, ref;
            
            set.add(1);
            ref = {1};
            Assert::Value(set) == ref;
            
            set.add({1, 3});
            ref = {1, 3};
            Assert::Value(set) == ref;
            
            int array[] = {1, 2, 4, 8};
            set.add(&array[0], &array[4]);
            ref = {1, 2, 3, 4, 8};
            Assert::Value(set) == ref;
            
            set.add(other);
            ref = {1, 2, 3, 4, 8};
            Assert::Value(set) == ref;
        });
        
        test("Remove", [](){
            StandardHashSet<int> set {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}, other = {2, 4, 6}, ref;
            
            set.remove(4);
            ref = {1, 2, 3, 5, 6, 7, 8, 9, 10};
            Assert::Value(set) == ref;
            
            set.remove({7, 8, 10});
            ref = {1, 2, 3, 5, 6, 9};
            Assert::Value(set) == ref;
            
            int array[] = {0, 3, 4, 10, 11};
            set.remove(&array[0], &array[5]);
            ref = {1, 2, 5, 6, 9};
            Assert::Value(set) == ref;
            
            set.remove(other);
            ref = {1, 5, 9};
            Assert::Value(set) == ref;
        });
        
        test("Intersect", [](){
            StandardHashSet<int>
                set1 {1, 2, 4, 8, 16},
                set2 {2, 4, 6, 8, 10, 12, 14, 16},
                ref {2, 4, 8, 16};
            set1.intersect(set2);
            Assert::Value(set1) == ref;
        });
    });
}
