////////////////////////////////////////////////////////////////////////////////
// Unit Test of the Neuro Hash Set implementations.
// -----
// Copyright (c) Kiruse 2018
// License: GPL 3.0
#include <iostream>

#include "Assert.hpp"
#include "NeuroMap.hpp"
#include "NeuroTestingFramework.hpp"

void TestNeuroMap() {
    using namespace std;
    using namespace Neuro;
    using namespace Neuro::Testing;
    
    section("Neuro Standard Hash Map", [](){
        test("Basic Functionality", [](){
            StandardHashMap<const char*, int> map;
            NEURO_ASSERT_EXPR(map.count()) == 0;
            
            map["test"] = 4;
            NEURO_ASSERT_EXPR(map.count()) == 1;
            NEURO_ASSERT_EXPR(map["test"]) == 4;
            
            map["test"] = 6;
            NEURO_ASSERT_EXPR(map.count()) == 1;
            NEURO_ASSERT_EXPR(map["test"]) == 6;
            
            map["foo"] = 12;
            map["bar"] = 16;
            NEURO_ASSERT_EXPR(map.count()) == 3;
            NEURO_ASSERT_EXPR(map["test"]) == 6;
            NEURO_ASSERT_EXPR(map["foo"]) == 12;
            NEURO_ASSERT_EXPR(map["bar"]) == 16;
        });
    });
}
