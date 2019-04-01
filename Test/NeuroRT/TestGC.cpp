////////////////////////////////////////////////////////////////////////////////
// Possibly the most complex "unit test" we have, although ironically it's not
// a unit test. It's more of a stress test, because eventually the GC will
// involve a more or less complex AI. And as with everything complex like that,
// it would become increasingly difficult to write a suitable unit test for it.
// Accordingly, I deemed a stress test more useful as it throws thousands of
// scenarios at the garbage collector and verifies that, even after a prolongued
// period, everything still works.
// -----
// Copyright (c) Kiruse 2018 Germany
// License: GPL 3.0
#include "GC/NeuroGC.hpp"
#include "NeuroObject.hpp"

#include <chrono>
#include <iostream>
#include <random>
#include <sstream>

using namespace Neuro;
using namespace Neuro::Runtime;


constexpr uint32 ITERATIONS = 100000;


Pointer createListObject() {
    Pointer list = Object::createObject(8, 8);
    list->getProperty("length") = (uint32)0;
    return list;
}

uint32 appendList(Pointer list, Value val) {
    std::stringstream ss;
    
    uint32 length = list->getProperty("length").getUInt();
    ss << length;
    
    list->getProperty(ss.str().c_str()) = val;
    list->getProperty("length") = length + 1;
    return length;
}

uint32 length(Pointer obj) {
    if (!obj->hasProperty("length")) return -1;
    
    Value val = obj->getProperty("length");
    return val.isUnsigned() ? val.getUInt() : (uint32)val.getInt();
}

Pointer createNeuron() {
    Pointer neuron = Object::createObject(8);
    neuron->getProperty("links") = createListObject();
    return neuron;
}

void link(Pointer neuron1, Pointer neuron2) {
    if (!neuron1->getProperty("links").isManagedObject() || !neuron2->getProperty("links").isManagedObject()) return;
    appendList(neuron1->getProperty("links").getManagedObject(), neuron2);
    appendList(neuron2->getProperty("links").getManagedObject(), neuron1);
}


std::default_random_engine rndgen(42000);
Pointer getRandomNeuron(Pointer root) {
    //std::default_random_engine rndgen((uint32)std::chrono::system_clock::now().time_since_epoch().count());
    std::uniform_real_distribution<float> rndPickThis;
    Pointer curr = root;
    
    while (rndPickThis(rndgen) >= 0.25f) {
        uint32 len = length(curr->getProperty("links").getManagedObject());
        if (!len) return curr;
        std::uniform_int_distribution rnd(0u, len - 1);
        
        std::stringstream ss;
        ss << rnd(rndgen);
        curr = curr->getProperty("links").getManagedObject()->getProperty(ss.str().c_str()).getManagedObject();
    }
    return curr;
}


int main(int argc, char** argv)
{
    GC::init();

	return 0;
    
    Pointer root = createNeuron();
	Pointer prev = root;
    root->root();
    
    for (uint32 i = 0; i < 10000; ++i) {
        Pointer neuron = createNeuron();
        //Pointer parent = getRandomNeuron(root);
        link(prev, neuron);
    }
    
    GC::destroy();
    
    std::cout << "Wooh! :O" << std::endl;
}
