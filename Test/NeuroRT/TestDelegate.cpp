////////////////////////////////////////////////////////////////////////////////
// Unit Test of the Neuro Hash Set implementations.
// -----
// Copyright (c) Kiruse 2018
// License: GPL 3.0
#include <iostream>

#include "Assert.hpp"
#include "Delegate.hpp"
#include "NeuroTestingFramework.hpp"

int foo(int num) {
    return num + 4;
}

class TestClass {
    int param;
    
public:
    TestClass(int value) : param(value) {}
    
    int publicMethod(int num) {
        return param + num;
    }
    
private:
    int privateMethod(int num) {
        return param - num;
    }
    
public:
    static int bar(int num) {
        return num - 4;
    }
    
    Neuro::Delegate<int, int>::MethodDelegate<TestClass, &TestClass::privateMethod> getPrivateDelegate() {
        return Neuro::Delegate<int, int>::MethodDelegate<TestClass, &TestClass::privateMethod>(this);
    }
};

void TestNeuroDelegate() {
    using namespace std;
    using namespace Neuro;
    using namespace Neuro::Testing;
    
    section("Neuro Delegate", [](){
        test("Function Delegate", [](){
            Delegate<int, int>::FunctionDelegate<foo> deleg1;
            NEURO_ASSERT_EXPR(deleg1(0)) == 4;
            NEURO_ASSERT_EXPR(deleg1(5)) == 9;
            
            Delegate<int, int>::FunctionDelegate<TestClass::bar> deleg2;
            NEURO_ASSERT_EXPR(deleg2(0)) == -4;
            NEURO_ASSERT_EXPR(deleg2(5)) == 1;
        });
        
        test("Method Delegate", [](){
            TestClass inst(24);
            
            Delegate<int, int>::MethodDelegate<TestClass, &TestClass::publicMethod> deleg1(&inst);
            NEURO_ASSERT_EXPR(deleg1(45)) == 69;
            
            auto deleg2 = inst.getPrivateDelegate();
            NEURO_ASSERT_EXPR(deleg2(45)) == -21;
        });
        
        test("Lambda Delegate", [](){
            int value = 4;
            
            auto deleg1 = Delegate<int, int>::fromLambda([&value](int val){
                return value += val;
            });
            
            auto deleg2 = Delegate<int, int>::fromLambda([&value](int val){
                return value -= val;
            });
            
            NEURO_ASSERT_EXPR(deleg1(24)) == 28;
            NEURO_ASSERT_EXPR(deleg2(32)) == -4;
        });
        
        test("Multicast Delegate", [](){
            MulticastDelegate<int, int> multi;
            TestClass inst(36);
            
            multi += Delegate<int, int>::FunctionDelegate<foo>();
            multi += Delegate<int, int>::FunctionDelegate<TestClass::bar>();
            multi += Delegate<int, int>::MethodDelegate<TestClass, &TestClass::publicMethod>(&inst);
            multi += inst.getPrivateDelegate();
            multi += Delegate<int, int>::fromLambda([](int val){
                return val * 3 / 2;
            });
            
            auto results = multi(24);
            NEURO_ASSERT_EXPR(results.length()) == 5;
            NEURO_ASSERT_EXPR(results[0]) == 28;
            NEURO_ASSERT_EXPR(results[1]) == 20;
            NEURO_ASSERT_EXPR(results[2]) == 60;
            NEURO_ASSERT_EXPR(results[3]) == 12;
            NEURO_ASSERT_EXPR(results[4]) == 36;
            
            multi -= Delegate<int, int>::FunctionDelegate<foo>();
            multi -= Delegate<int, int>::MethodDelegate<TestClass, &TestClass::publicMethod>(&inst);
			results = multi(24);
            NEURO_ASSERT_EXPR(results.length()) == 3;
            NEURO_ASSERT_EXPR(results[0]) == 20;
            NEURO_ASSERT_EXPR(results[1]) == 12;
            NEURO_ASSERT_EXPR(results[2]) == 36;
        });
    });
}
