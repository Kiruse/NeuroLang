////////////////////////////////////////////////////////////////////////////////
// Unit Test of the Neuro Maybe data type wrapper.
// -----
// Copyright (c) Kiruse 2018
// License: GPL 3.0
#include <iostream>

#include "Assert.hpp"
#include "Maybe.hpp"
#include "NeuroTestingFramework.hpp"
#include "Numeric.hpp"

struct Foo {
	int value;

	Foo() : value(4) {}
	Foo(int value) : value(value) {}
	Foo(const Foo& other) : value(other.value) {}
	Foo(Foo&& other) : value(other.value) {
		other.value = 245542;
	}
	~Foo() {
		value = 696969;
	}
};

void TestNeuroMaybe() {
    using namespace std;
    using namespace Neuro;
    using namespace Neuro::Testing;
    
    section("Neuro::Maybe", [](){
        test("Default Construction", [](){
            Maybe<int> maybe1;
            Maybe<Foo> maybe2;
            
            NEURO_ASSERT_EXPR((bool)maybe1) == false;
            NEURO_ASSERT_EXPR((bool)maybe2) == false;
            
            maybe2.create();
            NEURO_ASSERT_EXPR((bool)maybe2) == true;
            Assert::Value("maybe2", maybe2->value) == 4;
            
            maybe2.clear();
            Assert::Value("maybe2", maybe2->value) == 696969;
        });
        
        test("Copying", [](){
            Maybe<int> maybe1 = 4;
            Maybe<Foo> maybe2;
            maybe2.create();
            
            NEURO_ASSERT_EXPR(*maybe1) == 4;
            NEURO_ASSERT_EXPR(maybe2->value) == 4;
            
            Foo copy(23);
            maybe2 = copy;
            NEURO_ASSERT_EXPR(maybe2->value) == 23;
        });
        
        test("Moving", [](){
            Maybe<Foo> maybe(std::move(Foo(42)));
            NEURO_ASSERT_EXPR(maybe->value) == 42;
            
            maybe = std::move(Foo(36));
            NEURO_ASSERT_EXPR(maybe->value) == 36;
        });
        
        test("Forwarded Construction", [](){
            Maybe<Foo> maybe;
            maybe.create(72);
            NEURO_ASSERT_EXPR(maybe->value) == 72;
        });
    });
}
